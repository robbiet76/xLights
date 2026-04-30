#import "SoundClassifier.h"

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <SoundAnalysis/SoundAnalysis.h>

#include "AudioManager.h"

#include <algorithm>
#include <unordered_map>

// Observer that accumulates classification results by class name.
@interface XLSoundClassifierObserver : NSObject<SNResultsObserving>
@property(nonatomic, strong) NSMutableDictionary<NSString*, NSMutableArray<NSNumber*>*>* buckets;
@property(nonatomic, assign) BOOL didError;
@end

@implementation XLSoundClassifierObserver
- (instancetype)init {
    self = [super init];
    if (self) {
        _buckets = [NSMutableDictionary dictionary];
        _didError = NO;
    }
    return self;
}
- (void)request:(id<SNRequest>)request didProduceResult:(id<SNResult>)result {
    if (![result isKindOfClass:[SNClassificationResult class]]) return;
    SNClassificationResult* r = (SNClassificationResult*)result;
    for (SNClassification* c in r.classifications) {
        NSMutableArray<NSNumber*>* arr = self.buckets[c.identifier];
        if (!arr) {
            arr = [NSMutableArray array];
            self.buckets[c.identifier] = arr;
        }
        [arr addObject:@(c.confidence)];
    }
}
- (void)request:(id<SNRequest>)request didFailWithError:(NSError*)error {
    self.didError = YES;
}
- (void)requestDidComplete:(id<SNRequest>)request {
    // nothing to do — results were accumulated incrementally
}
@end

namespace {

// Copy up to `frames` samples of the AudioManager's left channel into
// a freshly-created AVAudioPCMBuffer at the source sample rate. The
// analyzer accepts mono float32 buffers; we feed the left channel
// only (right channel would double work without materially changing
// the classifier output).
AVAudioPCMBuffer* MakeMonoFloatBuffer(AudioManager* audio, long startFrame, long frames) {
    AVAudioFormat* fmt = [[AVAudioFormat alloc]
        initStandardFormatWithSampleRate:(double)audio->GetRate()
                                channels:1];
    AVAudioPCMBuffer* buf = [[AVAudioPCMBuffer alloc]
        initWithPCMFormat:fmt
           frameCapacity:(AVAudioFrameCount)frames];
    if (!buf) return nil;
    buf.frameLength = (AVAudioFrameCount)frames;
    float* dst = buf.floatChannelData[0];
    float* src = audio->GetRawLeftDataPtr(startFrame);
    if (!src || !dst) return nil;
    memcpy(dst, src, sizeof(float) * (size_t)frames);
    return buf;
}

} // namespace

SoundClassification ClassifySound(AudioManager* audio,
                                   const SoundClassifierOptions& opts) {
    SoundClassification out;
    if (!audio || !audio->IsOk()) return out;

    long trackSize = audio->GetTrackSize();
    long rate = audio->GetRate();
    if (trackSize <= 0 || rate <= 0) return out;

    // Ensure raw data is fully loaded.
    (void)audio->GetRawLeftDataPtr(trackSize - 1);

    @autoreleasepool {
        NSError* err = nil;
        SNClassifySoundRequest* request = nil;
        if (@available(iOS 15.0, macOS 12.0, *)) {
            request = [[SNClassifySoundRequest alloc]
                initWithClassifierIdentifier:SNClassifierIdentifierVersion1
                                       error:&err];
        }
        if (!request || err) {
            return out;
        }
        if (opts.windowSeconds > 0) {
            if (@available(iOS 15.0, macOS 12.0, *)) {
                request.windowDuration = CMTimeMakeWithSeconds(
                    opts.windowSeconds, NSEC_PER_SEC);
            }
        }

        AVAudioFormat* fmt = [[AVAudioFormat alloc]
            initStandardFormatWithSampleRate:(double)rate
                                    channels:1];
        SNAudioStreamAnalyzer* analyzer = [[SNAudioStreamAnalyzer alloc]
            initWithFormat:fmt];
        if (!analyzer) return out;

        XLSoundClassifierObserver* observer = [[XLSoundClassifierObserver alloc] init];
        if (![analyzer addRequest:request withObserver:observer error:&err]) {
            return out;
        }

        // Feed the analyzer in ~1 second chunks. Smaller chunks would
        // be fine too; Apple's analyzer buffers internally.
        const long chunkFrames = std::max<long>(1, (long)rate);
        long pos = 0;
        while (pos < trackSize) {
            long n = std::min(chunkFrames, trackSize - pos);
            AVAudioPCMBuffer* pcm = MakeMonoFloatBuffer(audio, pos, n);
            if (!pcm) break;
            [analyzer analyzeAudioBuffer:pcm
                    atAudioFramePosition:(AVAudioFramePosition)pos];
            pos += n;
        }
        [analyzer completeAnalysis];
        if (observer.didError) {
            return out;
        }

        // Aggregate into SoundClassResult, filter + sort.
        std::vector<SoundClassResult> all;
        all.reserve(observer.buckets.count);
        for (NSString* key in observer.buckets) {
            NSArray<NSNumber*>* arr = observer.buckets[key];
            SoundClassResult r;
            r.name = [key UTF8String];
            r.confidence.reserve(arr.count);
            double sum = 0;
            for (NSNumber* n in arr) {
                float v = n.floatValue;
                r.confidence.push_back(v);
                sum += v;
            }
            if (!r.confidence.empty()) {
                r.averageConfidence = (float)(sum / double(r.confidence.size()));
            }
            if (r.averageConfidence >= opts.minAverageConfidence) {
                all.push_back(std::move(r));
            }
        }
        std::sort(all.begin(), all.end(),
                  [](const SoundClassResult& a, const SoundClassResult& b) {
                      return a.averageConfidence > b.averageConfidence;
                  });
        if ((int)all.size() > opts.maxClasses) {
            all.resize((size_t)opts.maxClasses);
        }
        out.classes = std::move(all);
        out.timeStepSeconds = opts.windowSeconds > 0 ? opts.windowSeconds : 1.0f;
        out.lengthMS = audio->LengthMS();
    }
    return out;
}
