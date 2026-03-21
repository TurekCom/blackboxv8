#include "blackbox/nvda_api.h"

#include "blackbox/sam_like.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

int ClampInt(int value, int lo, int hi) {
    return value < lo ? lo : (value > hi ? hi : value);
}

int PercentToSamSpeed(int percent) {
    const double centered = (static_cast<double>(ClampInt(percent, 0, 100)) - 50.0) / 50.0;
    const double factor = std::pow(3.0, -centered);
    return ClampInt(static_cast<int>(std::lround(72.0 * factor)), 24, 180);
}

int PercentToSamPitch(int percent) {
    const double centered = (static_cast<double>(ClampInt(percent, 0, 100)) - 50.0) / 50.0;
    const double factor = std::pow(2.0, -centered);
    return ClampInt(static_cast<int>(std::lround(64.0 * factor)), 24, 104);
}

int PercentToIntonationStrength(int percent) {
    return ClampInt(20 + static_cast<int>(std::lround(1.8 * ClampInt(percent, 0, 100))), 20, 200);
}

}  // namespace

int bbx_get_api_version() {
    return 1;
}

int bbx_get_sample_rate() {
    return 22050;
}

int bbx_synthesize_utf16(
    const wchar_t* text,
    int rate_percent,
    int pitch_percent,
    int volume_percent,
    int modulation_percent,
    unsigned char** out_pcm,
    uint32_t* out_size
) {
    if (!text || !out_pcm || !out_size) {
        return -1;
    }

    *out_pcm = nullptr;
    *out_size = 0;

    blackbox::SamVoiceSettings settings;
    settings.voice.speed = PercentToSamSpeed(rate_percent);
    settings.voice.pitch = PercentToSamPitch(pitch_percent);
    settings.voice.mouth = 128;
    settings.voice.throat = 128;
    settings.volume = ClampInt(volume_percent, 0, 100);
    settings.stress = 5;
    settings.intonationStrength = PercentToIntonationStrength(modulation_percent);
    settings.intonationMode = blackbox::SamIntonationMode::Auto;
    settings.quantizeToC64 = true;

    std::vector<uint8_t> pcm = blackbox::SynthesizePolishSamLike(text, settings);
    if (pcm.empty()) {
        return 0;
    }

    void* memory = std::malloc(pcm.size());
    if (!memory) {
        return -2;
    }
    std::memcpy(memory, pcm.data(), pcm.size());
    *out_pcm = static_cast<unsigned char*>(memory);
    *out_size = static_cast<uint32_t>(pcm.size());
    return 0;
}

void bbx_free_buffer(void* buffer) {
    std::free(buffer);
}
