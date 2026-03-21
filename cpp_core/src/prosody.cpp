#include "blackbox/prosody.hpp"

#include <algorithm>
#include <cwctype>

namespace blackbox {
namespace {

int ClampToByte(int value) {
    return std::max(0, std::min(255, value));
}

bool IsMark(wchar_t ch) {
    return ch == L'.' || ch == L'?' || ch == L'!' || ch == L',' || ch == L';' || ch == L':';
}

SentenceMark ParseMark(wchar_t ch) {
    switch (ch) {
    case L'?':
        return SentenceMark::Question;
    case L'!':
        return SentenceMark::Exclamation;
    case L',':
        return SentenceMark::Comma;
    case L';':
        return SentenceMark::Semicolon;
    case L':':
        return SentenceMark::Colon;
    case L'.':
    default:
        return SentenceMark::Statement;
    }
}

std::wstring Trim(std::wstring value) {
    while (!value.empty() && std::iswspace(value.front())) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::iswspace(value.back())) {
        value.pop_back();
    }
    return value;
}

}  // namespace

std::vector<ProsodyChunk> SplitProsodyChunks(std::wstring_view text) {
    std::vector<ProsodyChunk> out;
    std::wstring current;

    for (wchar_t ch : text) {
        current.push_back(ch);
        if (!IsMark(ch)) {
            continue;
        }

        const SentenceMark mark = ParseMark(ch);
        current.pop_back();
        std::wstring body = Trim(current);
        if (!body.empty()) {
            out.push_back(ProsodyChunk{body, mark});
        }
        current.clear();
    }

    std::wstring tail = Trim(current);
    if (!tail.empty()) {
        out.push_back(ProsodyChunk{tail, SentenceMark::Statement});
    }

    return out;
}

VoiceParams ApplyProsody(const VoiceParams& base, SentenceMark mark) {
    VoiceParams out = base;

    switch (mark) {
    case SentenceMark::Question:
        out.pitch = ClampToByte(out.pitch + 10);
        out.speed = ClampToByte(out.speed - 4);
        out.mouth = ClampToByte(out.mouth + 4);
        break;
    case SentenceMark::Exclamation:
        out.pitch = ClampToByte(out.pitch + 14);
        out.speed = ClampToByte(out.speed + 3);
        out.mouth = ClampToByte(out.mouth + 8);
        break;
    case SentenceMark::Comma:
    case SentenceMark::Semicolon:
    case SentenceMark::Colon:
        out.pitch = ClampToByte(out.pitch - 2);
        out.speed = ClampToByte(out.speed - 2);
        break;
    case SentenceMark::Statement:
    default:
        break;
    }

    return out;
}

wchar_t SamCompatibleMark(SentenceMark mark) {
    switch (mark) {
    case SentenceMark::Question:
        return L'?';
    case SentenceMark::Comma:
    case SentenceMark::Semicolon:
    case SentenceMark::Colon:
        return L',';
    case SentenceMark::Exclamation:
    case SentenceMark::Statement:
    default:
        return L'.';
    }
}

}  // namespace blackbox
