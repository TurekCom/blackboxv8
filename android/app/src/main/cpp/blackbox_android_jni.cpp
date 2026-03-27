#include <jni.h>

#include "blackbox/sam_like.hpp"

#include <cmath>
#include <string>
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

std::wstring JStringToWString(JNIEnv* env, jstring text) {
    if (text == nullptr) {
        return {};
    }
    const jchar* chars = env->GetStringChars(text, nullptr);
    const jsize length = env->GetStringLength(text);
    std::wstring out;
    out.reserve(static_cast<size_t>(length));
    for (jsize i = 0; i < length; ++i) {
        out.push_back(static_cast<wchar_t>(chars[i]));
    }
    env->ReleaseStringChars(text, chars);
    return out;
}

}  // namespace

extern "C" JNIEXPORT jint JNICALL
Java_com_turek_blackbox_android_NativeBlackBox_getSampleRate(JNIEnv*, jobject) {
    return 22050;
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_turek_blackbox_android_NativeBlackBox_synthesizePcm(
    JNIEnv* env,
    jobject,
    jstring text,
    jint ratePercent,
    jint pitchPercent,
    jint volumePercent,
    jint modulationPercent
) {
    blackbox::SamVoiceSettings settings;
    settings.voice.speed = PercentToSamSpeed(ratePercent);
    settings.voice.pitch = PercentToSamPitch(pitchPercent);
    settings.voice.mouth = 128;
    settings.voice.throat = 128;
    settings.volume = ClampInt(volumePercent, 0, 100);
    settings.stress = 5;
    settings.intonationStrength = PercentToIntonationStrength(modulationPercent);
    settings.intonationMode = blackbox::SamIntonationMode::Auto;
    settings.quantizeToC64 = true;

    std::vector<uint8_t> pcm = blackbox::SynthesizePolishSamLike(JStringToWString(env, text), settings);
    jbyteArray out = env->NewByteArray(static_cast<jsize>(pcm.size()));
    if (out == nullptr || pcm.empty()) {
        return out;
    }
    env->SetByteArrayRegion(out, 0, static_cast<jsize>(pcm.size()), reinterpret_cast<const jbyte*>(pcm.data()));
    return out;
}
