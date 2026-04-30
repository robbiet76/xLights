
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <wx/version.h>
#include <wx/platinfo.h>
#include <wx/utils.h>

#include <nlohmann/json.hpp>

#include "ai/aiBase.h"
#include "ai/ServiceProperty.h"
#include "ai/ServiceManager.h"

#include "utils/string_utils.h"
#include "xLights-Swift.h"
#include "AppleIntelligence.h"


std::list<aiType::TYPE> AppleIntelligence::GetTypes() const {
    // At this point, don't handle "PROMPT" as the session size limits
    // are too small for the crazy long input prompts needed for the mapping.
    // The on-device LLM (FoundationModels.LanguageModelSession) requires macOS 26.0+,
    // while ImagePlayground.ImageCreator only requires macOS 15.4+.
    std::list<aiType::TYPE> types;
    if (wxCheckOsVersion(26, 0, 0)) {
        types.push_back(aiType::TYPE::COLORPALETTES);
    }
    if (wxCheckOsVersion(15, 4, 0)) {
        types.push_back(aiType::TYPE::IMAGES);
    }
    return types;
}

bool AppleIntelligence::IsAvailable() const {
    return !_enabledTypes.empty();
}

void AppleIntelligence::SaveSettings() const {
    for (auto t : GetTypes()) {
        _sm->setServiceSetting(std::string("appleAIEnable_") + aiType::TypeSettingsSuffix(t), IsEnabledForType(t));
    }
}

void AppleIntelligence::LoadSettings() {
    bool oldEnabled = _sm->getServiceSetting("appleAIEnable", false);
    for (auto t : GetTypes()) {
        bool enabled = _sm->getServiceSetting(std::string("appleAIEnable_") + aiType::TypeSettingsSuffix(t), oldEnabled);
        SetEnabledForType(t, enabled);
    }
}

std::vector<ServiceProperty> AppleIntelligence::GetProperties() const {
    std::vector<ServiceProperty> props;
    props.push_back({ ServiceProperty::Kind::Category, {}, "Apple Intelligence", "AppleIntelligence", {}, {}, {} });
    for (auto t : GetTypes()) {
        props.push_back({
            ServiceProperty::Kind::Bool,
            std::string("AppleIntelligence.Enable_") + aiType::TypeSettingsSuffix(t),
            std::string("Enable ") + aiType::TypeName(t),
            "AppleIntelligence",
            {},
            {},
            IsEnabledForType(t)
        });
    }
    return props;
}

void AppleIntelligence::SetProperty(const std::string& id, bool value) {
    for (auto t : GetTypes()) {
        if (id == std::string("AppleIntelligence.Enable_") + aiType::TypeSettingsSuffix(t)) {
            SetEnabledForType(t, value);
            return;
        }
    }
}

std::pair<std::string, bool> AppleIntelligence::CallLLM(const std::string& prompt) const {
    std::string s = xLights::RunAppleIntelligencePrompt(prompt);

    return {s, !s.empty()};
}

aiBase::AIColorPalette AppleIntelligence::GenerateColorPalette(const std::string &prompt) const {
    aiBase::AIColorPalette ret;

    std::string res = xLights::RunAppleIntelligenceGeneratePalette(prompt);
    if (!res.empty()) {

        try {
            // Check if the response is valid JSON
            nlohmann::json const root = nlohmann::json::parse(res);
            if (root.contains("error")) {
                ret.error = root["error"].get<std::string>();
            } else {
                ret.description = root["Description"].get<std::string>();
                for (int x = 0; x < root["Colors"].size(); x++) {
                    ret.colors.push_back(aiBase::AIColor());
                    ret.colors.back().description = root["Colors"][x]["Description"].get<std::string>();
                    ret.colors.back().name = root["Colors"][x]["Name"].get<std::string>();
                    ret.colors.back().hexValue = root["Colors"][x]["Hex Value"].get<std::string>();
                    if (!ret.colors.back().hexValue.empty() &&  ret.colors.back().hexValue[0] != '#') {
                        ret.colors.back().hexValue = "#" + ret.colors.back().hexValue;
                    }
                }
            }
        } catch (const std::exception& ex) {

        }
    }
    return ret;
}


// Encode a CGImage to an in-memory PNG byte buffer.
static std::vector<uint8_t> CGImageToPNGBytes(CGImageRef image) {
    std::vector<uint8_t> bytes;
    if (!image) return bytes;

    CFMutableDataRef data = CFDataCreateMutable(nullptr, 0);
    if (!data) return bytes;

    if (@available(macOS 11.0, *)) {
        CGImageDestinationRef dest = CGImageDestinationCreateWithData(data, (__bridge CFStringRef)UTTypePNG.identifier, 1, nullptr);
        if (!dest) {
            CFRelease(data);
            return bytes;
        }
        
        CGImageDestinationAddImage(dest, image, nullptr);
        if (CGImageDestinationFinalize(dest)) {
            const uint8_t* p = CFDataGetBytePtr(data);
            CFIndex len = CFDataGetLength(data);
            bytes.assign(p, p + len);
        }
        CFRelease(dest);
    } else {
        // Fallback on earlier versions
    }
    CFRelease(data);
    return bytes;
}


namespace {

constexpr const char* kAppleStyleId = "AppleIntelligence.Style";
constexpr const char* kAppleStyleCategory = "Apple Intelligence Image";

class AppleIntelligenceImageGenerator : public aiBase::AIImageGenerator {
public:
    ~AppleIntelligenceImageGenerator() override = default;

    std::vector<ServiceProperty> GetProperties() const override {
        ServiceProperty p;
        p.kind = ServiceProperty::Kind::Choice;
        p.id = kAppleStyleId;
        p.label = "Style";
        p.category = kAppleStyleCategory;
        p.choices = { "animation", "illustration", "sketch", "emoji" };
        p.value = style;
        return { p };
    }

    void SetProperty(const std::string& id, const std::string& value) override {
        if (id == kAppleStyleId) {
            style = Lower(value);
        }
    }

    void generateImage(const std::string &prompt,
                       std::function<void(aiBase::AIImageResult)> cb) override {
        callback = std::move(cb);

        std::string full = prompt + R"(
        MANDATORY OUTPUT REQUIREMENTS: Background: Black background (#000000) with no watermarks or border.
        The design features bold, clean outlines, simple cell-shading, and a limited vibrant color palette with clean edges and no gradients.
        )";


        NSString *p = @(prompt.c_str());
        ImagesAsyncCaller *caller = [[ImagesAsyncCaller alloc] init];

        [caller generateImagesWithPrompt:p fullInstructions:@(full.c_str()) style:@(style.c_str()) completionHandler:^(CGImage *result, NSString *errString) {
            aiBase::AIImageResult res;
            std::string err = errString ? std::string([errString UTF8String]) : std::string();
            if (!err.empty()) {
                res.error = err;
            } else {
                res.pngBytes = CGImageToPNGBytes(result);
                if (res.pngBytes.empty()) {
                    res.error = "Failed to encode generated image to PNG";
                }
            }
            if (callback) callback(std::move(res));
        }];
    }

    std::function<void(aiBase::AIImageResult)> callback;
    std::string style = "animation";
};

} // namespace

aiBase::AIImageGenerator *AppleIntelligence::createAIImageGenerator() const {
    return new AppleIntelligenceImageGenerator();
}
