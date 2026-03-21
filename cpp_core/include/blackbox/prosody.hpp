#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace blackbox {

enum class SentenceMark {
    Statement,
    Question,
    Exclamation,
    Comma,
    Semicolon,
    Colon,
};

struct VoiceParams {
    int speed = 72;
    int pitch = 64;
    int mouth = 128;
    int throat = 128;
};

struct ProsodyChunk {
    std::wstring text;
    SentenceMark mark = SentenceMark::Statement;
};

std::vector<ProsodyChunk> SplitProsodyChunks(std::wstring_view text);
VoiceParams ApplyProsody(const VoiceParams& base, SentenceMark mark);
wchar_t SamCompatibleMark(SentenceMark mark);

}  // namespace blackbox
