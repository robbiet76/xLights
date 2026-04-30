#import "StemSeparator.h"

#import <Foundation/Foundation.h>
#import <CoreML/CoreML.h>

#include "AudioManager.h"
#include "kiss_fft/tools/kiss_fftr.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace {

// HTDemucs STFT parameters (baked into the model at conversion).
constexpr int kSTFT_NFFT = 4096;
constexpr int kSTFT_HOP  = 1024;
constexpr int kSTFT_BINS = 2048;     // n_fft/2, Nyquist cropped
constexpr int kSTFT_FRAMES = 336;    // matches the model's `spectral_magnitude` T
constexpr int kSTFT_PAD_LEFT = 1536;  // hl/2 * 3
// pad_right chosen so center=False STFT yields exactly 336 frames:
//   (pad_left + N + pad_right - n_fft) / hop + 1 = 336
//   → pad_right = 336*hop + n_fft - N - pad_left - 1? actually:
//   padded_len = (frames - 1) * hop + n_fft = 335*1024 + 4096 = 347136
constexpr int kSTFT_PADDED_LEN = 347136;

// Helper: make a Float16-backed MLMultiArray. HTDemucs I/O is Float16.
MLMultiArray* MakeFloat16Array(NSArray<NSNumber*>* shape, NSError** error) {
    if (@available(macOS 12.0, iOS 15.0, *)) {
        return [[MLMultiArray alloc]
            initWithShape:shape
                 dataType:MLMultiArrayDataTypeFloat16
                    error:error];
    }
    *error = [NSError errorWithDomain:@"xLights.StemSeparator"
                                 code:1
                             userInfo:@{NSLocalizedDescriptionKey:
                                 @"CoreML Float16 multi-array requires macOS 12 / iOS 15"}];
    return nil;
}

// Copy a stereo slice into the audio_waveform input [1, 2, chunk],
// Float32 → Float16.
void FillWaveformArray(MLMultiArray* wf,
                        const float* L, const float* R,
                        long start, long count, long chunkFrames) {
    NSArray<NSNumber*>* strides = wf.strides;
    const long strideCh = strides[1].longValue;
    __fp16* data = (__fp16*)wf.dataPointer;
    for (long i = 0; i < chunkFrames; i++) {
        float lval = (i < count) ? L[start + i] : 0.0f;
        float rval = (i < count && R) ? R[start + i] : lval;
        data[0 * strideCh + i] = (__fp16)lval;
        data[1 * strideCh + i] = (__fp16)rval;
    }
}

// Compute HTDemucs's "spectral_magnitude" input from the stereo chunk.
// Despite the name, the model expects the *complex* STFT stacked as 4
// float channels: [L_real, L_imag, R_real, R_imag] × 2048 bins × 336
// frames. Hann-windowed STFT with n_fft=4096, hop=1024, center=False,
// left-pad = 1536, total padded length = 347136 samples.
void FillSpectralArray(MLMultiArray* spec,
                        const float* L, const float* R,
                        long start, long validCount, long chunkFrames,
                        kiss_fftr_cfg cfg,
                        std::vector<float>& window,
                        std::vector<float>& padded,
                        std::vector<float>& frame,
                        std::vector<kiss_fft_cpx>& fftOut) {
    NSArray<NSNumber*>* strides = spec.strides;
    const long strideC = strides[1].longValue;
    const long strideF = strides[2].longValue;
    const long strideT = strides[3].longValue;
    __fp16* data = (__fp16*)spec.dataPointer;

    for (int ch = 0; ch < 2; ch++) {
        const float* src = (ch == 0) ? L : R;
        // Zero-init the padded buffer, copy chunk into [pad_left .. pad_left+validCount).
        std::fill(padded.begin(), padded.end(), 0.0f);
        for (long i = 0; i < validCount; i++) {
            padded[kSTFT_PAD_LEFT + i] = src[start + i];
        }

        for (int t = 0; t < kSTFT_FRAMES; t++) {
            const int frameStart = t * kSTFT_HOP;
            for (int k = 0; k < kSTFT_NFFT; k++) {
                frame[k] = padded[frameStart + k] * window[k];
            }
            kiss_fftr(cfg, frame.data(), fftOut.data());
            // Write first 2048 complex values as two adjacent channel rows.
            const long chReal = (ch * 2) + 0; // L: 0, R: 2
            const long chImag = (ch * 2) + 1; // L: 1, R: 3
            for (int k = 0; k < kSTFT_BINS; k++) {
                data[chReal * strideC + k * strideF + t * strideT] = (__fp16)fftOut[k].r;
                data[chImag * strideC + k * strideF + t * strideT] = (__fp16)fftOut[k].i;
            }
        }
    }
}

// Pull an 8-channel slice out of time_output at [1, 8, chunkFrames]
// into pre-sized destination vectors at `dstOffset`, crossfaded with
// any prior content via the `fadeIn` ramp (length = overlap).
void AppendOutputs(MLMultiArray* out,
                    long chunkFrames, long validCount,
                    long dstOffset, long overlap,
                    std::vector<float>& drumsL, std::vector<float>& drumsR,
                    std::vector<float>& bassL,  std::vector<float>& bassR,
                    std::vector<float>& otherL, std::vector<float>& otherR,
                    std::vector<float>& vocalsL, std::vector<float>& vocalsR) {
    NSArray<NSNumber*>* strides = out.strides;
    const long strideCh = strides[1].longValue;
    __fp16* data = (__fp16*)out.dataPointer;
    // Source order in `time_output`. The john-rocky model card claims
    // drums, bass, other, vocals but the actual converted model emits
    // drums, bass, vocals, other — verified empirically.
    std::vector<float>* targets[8] = {
        &drumsL,  &drumsR,
        &bassL,   &bassR,
        &vocalsL, &vocalsR,
        &otherL,  &otherR
    };
    for (int ch = 0; ch < 8; ch++) {
        std::vector<float>& dst = *targets[ch];
        const __fp16* row = data + ch * strideCh;
        for (long i = 0; i < validCount; i++) {
            long out_i = dstOffset + i;
            if (out_i < 0 || out_i >= long(dst.size())) continue;
            float v = float(row[i]);
            if (overlap > 0 && i < overlap && out_i < long(dst.size())) {
                // Linear crossfade with whatever was written by the
                // previous chunk's tail. `i` runs 0..overlap-1.
                float t = float(i) / float(overlap);
                dst[out_i] = dst[out_i] * (1.0f - t) + v * t;
            } else {
                dst[out_i] = v;
            }
        }
    }
}

} // namespace

bool SeparateStems(AudioManager* audio,
                    const std::string& modelPath,
                    StemOutput& out,
                    const StemSeparatorOptions& opts,
                    std::function<void(int pct)> progress) {
    if (!audio || !audio->IsOk()) return false;
    if (modelPath.empty()) return false;

    // The HTDemucs model uses Float16 MLMultiArray I/O, which
    // requires macOS 12 / iOS 15. xLights's deployment target is
    // 10.15, so gate the whole path at runtime.
    if (@available(macOS 12.0, iOS 15.0, *)) {
        // ok — fall through
    } else {
        NSLog(@"StemSeparator: requires macOS 12 / iOS 15 or newer");
        return false;
    }

    long trackSize = audio->GetTrackSize();
    long rate = audio->GetRate();
    if (trackSize <= 0 || rate <= 0) return false;

    // Wait for the audio to finish loading before we start pulling
    // samples — the separator iterates the whole track.
    (void)audio->GetRawLeftDataPtr(trackSize - 1);
    const float* srcL = audio->GetRawLeftDataPtr(0);
    const float* srcR = audio->GetRawRightDataPtr(0);
    if (!srcL) return false;
    if (!srcR) srcR = srcL; // mono safety

    @autoreleasepool {
        NSString* path = [NSString stringWithUTF8String:modelPath.c_str()];
        NSURL* url = [NSURL fileURLWithPath:path];
        NSError* err = nil;

        // Compile the mlpackage if it isn't already. `compileModelAtURL`
        // caches the compiled artifact alongside the app sandbox.
        NSURL* compiledURL = nil;
        if ([path hasSuffix:@".mlmodelc"]) {
            compiledURL = url;
        } else {
            compiledURL = [MLModel compileModelAtURL:url error:&err];
            if (!compiledURL || err) {
                NSLog(@"StemSeparator: compile failed: %@", err);
                return false;
            }
        }
        MLModelConfiguration* cfg = [[MLModelConfiguration alloc] init];
        // `computeUnits = All` lets CoreML choose CPU / GPU / ANE.
        cfg.computeUnits = MLComputeUnitsAll;
        MLModel* model = [MLModel modelWithContentsOfURL:compiledURL
                                           configuration:cfg
                                                   error:&err];
        if (!model || err) {
            NSLog(@"StemSeparator: model load failed: %@", err);
            return false;
        }

        const long chunkFrames = opts.chunkSamples;
        const long overlap = std::max<long>(0, std::min<long>(opts.overlapSamples, chunkFrames / 2));
        const long stride = chunkFrames - overlap;
        if (stride <= 0) return false;

        // Pre-size outputs to the track length.
        out.drumsL.assign(trackSize, 0.0f);
        out.drumsR.assign(trackSize, 0.0f);
        out.bassL.assign(trackSize, 0.0f);
        out.bassR.assign(trackSize, 0.0f);
        out.otherL.assign(trackSize, 0.0f);
        out.otherR.assign(trackSize, 0.0f);
        out.vocalsL.assign(trackSize, 0.0f);
        out.vocalsR.assign(trackSize, 0.0f);
        out.sampleRate = rate;

        MLMultiArray* waveform = MakeFloat16Array(@[@1, @2, @(chunkFrames)], &err);
        if (!waveform || err) {
            NSLog(@"StemSeparator: audio_waveform alloc failed: %@", err);
            return false;
        }
        MLMultiArray* spectral = MakeFloat16Array(
            @[@1, @4, @(kSTFT_BINS), @(kSTFT_FRAMES)], &err);
        if (!spectral || err) {
            NSLog(@"StemSeparator: spectral_magnitude alloc failed: %@", err);
            return false;
        }

        // STFT scratch buffers (reused per chunk).
        kiss_fftr_cfg fftCfg = kiss_fftr_alloc(kSTFT_NFFT, 0, nullptr, nullptr);
        if (!fftCfg) {
            NSLog(@"StemSeparator: kiss_fftr_alloc failed");
            return false;
        }
        std::vector<float> window(kSTFT_NFFT);
        for (int i = 0; i < kSTFT_NFFT; i++) {
            window[i] = 0.5f * (1.0f - std::cos(2.0f * float(M_PI) * float(i) / float(kSTFT_NFFT - 1)));
        }
        std::vector<float> padded(kSTFT_PADDED_LEN, 0.0f);
        std::vector<float> frame(kSTFT_NFFT, 0.0f);
        std::vector<kiss_fft_cpx> fftOut(kSTFT_NFFT / 2 + 1);

        // Chunk loop.
        long totalChunks = (trackSize + stride - 1) / stride;
        long chunkIdx = 0;
        for (long srcPos = 0; srcPos < trackSize; srcPos += stride) {
            long validCount = std::min<long>(chunkFrames, trackSize - srcPos);
            FillWaveformArray(waveform, srcL, srcR, srcPos, validCount, chunkFrames);
            FillSpectralArray(spectral, srcL, srcR, srcPos, validCount, chunkFrames,
                               fftCfg, window, padded, frame, fftOut);

            MLDictionaryFeatureProvider* input =
                [[MLDictionaryFeatureProvider alloc]
                    initWithDictionary:@{@"audio_waveform": waveform,
                                         @"spectral_magnitude": spectral}
                                  error:&err];
            if (!input || err) {
                NSLog(@"StemSeparator: input provider failed: %@", err);
                return false;
            }
            id<MLFeatureProvider> output =
                [model predictionFromFeatures:input error:&err];
            if (!output || err) {
                NSLog(@"StemSeparator: inference failed: %@", err);
                return false;
            }
            MLFeatureValue* timeValue = [output featureValueForName:@"time_output"];
            if (!timeValue || timeValue.type != MLFeatureTypeMultiArray) {
                NSLog(@"StemSeparator: missing time_output tensor");
                return false;
            }
            MLMultiArray* timeOut = timeValue.multiArrayValue;
            // Guard against shape surprises — we expect [1, 8, N].
            if (timeOut.shape.count < 3 || timeOut.shape[1].longValue != 8) {
                NSLog(@"StemSeparator: unexpected output shape %@", timeOut.shape);
                return false;
            }
            AppendOutputs(timeOut, chunkFrames, validCount, srcPos,
                           (chunkIdx == 0 ? 0 : overlap),
                           out.drumsL, out.drumsR,
                           out.bassL,  out.bassR,
                           out.otherL, out.otherR,
                           out.vocalsL, out.vocalsR);

            chunkIdx++;
            if (progress) {
                int pct = int((chunkIdx * 100) / std::max<long>(1, totalChunks));
                if (pct > 100) pct = 100;
                progress(pct);
            }
        }
        free(fftCfg);
    }
    return true;
}
