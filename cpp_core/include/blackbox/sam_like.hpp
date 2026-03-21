#pragma once

#include "blackbox/prosody.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace blackbox {

enum class SamIntonationMode {
    Auto,
    Flat,
    Statement,
    Question,
    Exclamation,
};

struct SamVoiceSettings {
    VoiceParams voice{};
    int volume = 100;
    int stress = 5;
    int intonationStrength = 100;
    SamIntonationMode intonationMode = SamIntonationMode::Auto;
    bool quantizeToC64 = true;
};

std::vector<std::string> TextToSamPhonemes(std::wstring_view text, int stress = 5);
std::wstring DebugSamPhonemes(std::wstring_view text, int stress = 5);
std::vector<uint8_t> SynthesizePolishSamLike(std::wstring_view text, const SamVoiceSettings& settings);

}  // namespace blackbox
