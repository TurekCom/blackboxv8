#include "blackbox/sam_like.hpp"

#include "blackbox/text.hpp"

extern "C" {
#include "sam.h"
}

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <random>
#include <string>
#include <string_view>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace blackbox {
namespace {

constexpr uint32_t kSampleRate = 22050;

struct SamUnit {
    std::string phoneme;
    bool isVowel = false;
};

struct Lexeme {
    enum class Kind {
        Word,
        Punctuation,
    };
    Kind kind = Kind::Word;
    std::wstring text;
};

bool IsWordChar(wchar_t ch) {
    return (ch >= L'a' && ch <= L'z') || ch == L'ą' || ch == L'ć' || ch == L'ę' || ch == L'ł' ||
           ch == L'ń' || ch == L'ó' || ch == L'ś' || ch == L'ź' || ch == L'ż';
}

bool IsPolishVowel(wchar_t ch) {
    return ch == L'a' || ch == L'ą' || ch == L'e' || ch == L'ę' || ch == L'i' || ch == L'o' ||
           ch == L'u' || ch == L'ó' || ch == L'y';
}

bool IsPolishVowelWithI(wchar_t ch) {
    return IsPolishVowel(ch) || ch == L'i';
}

bool IsVoicelessRzPrefix(wchar_t ch) {
    return ch == L'p' || ch == L't' || ch == L'k' || ch == L'f' || ch == L'h' || ch == L's' ||
           ch == L'c' || ch == L'ć';
}

void ReplaceAll(std::wstring& s, const std::wstring& from, const std::wstring& to) {
    if (from.empty()) {
        return;
    }
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::wstring::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

std::vector<Lexeme> TokenizeText(std::wstring_view text) {
    std::vector<Lexeme> out;
    const std::wstring src = ToLowerPL(text);
    size_t i = 0;
    while (i < src.size()) {
        const wchar_t ch = src[i];
        if (std::iswspace(ch)) {
            ++i;
            continue;
        }
        if (IsWordChar(ch)) {
            size_t j = i + 1;
            while (j < src.size() && IsWordChar(src[j])) {
                ++j;
            }
            out.push_back({Lexeme::Kind::Word, src.substr(i, j - i)});
            i = j;
            continue;
        }
        out.push_back({Lexeme::Kind::Punctuation, std::wstring(1, ch)});
        ++i;
    }
    return out;
}

std::wstring SoftenSequences(std::wstring word) {
    static const std::array<std::pair<const wchar_t*, const wchar_t*>, 5> kPatterns = {{
        {L"dzi", L"dźi"},
        {L"ci", L"ći"},
        {L"si", L"śi"},
        {L"zi", L"źi"},
        {L"ni", L"ńi"},
    }};
    for (const auto& [from, to] : kPatterns) {
        ReplaceAll(word, from, to);
    }
    return word;
}

std::wstring NormalizeNasalVowels(std::wstring_view word) {
    std::wstring out;
    out.reserve(word.size() + 4);
    for (size_t i = 0; i < word.size(); ++i) {
        const wchar_t ch = word[i];
        const wchar_t nxt = (i + 1 < word.size()) ? word[i + 1] : L'\0';
        if (ch == L'ą') {
            if (nxt == L'p' || nxt == L'b' || nxt == L'm' || nxt == L'f' || nxt == L'w' || nxt == L'v') {
                out += L"om";
            } else if (nxt != L'\0' && !IsPolishVowel(nxt)) {
                out += L"on";
            } else {
                out += L"o";
            }
            continue;
        }
        if (ch == L'ę') {
            if (nxt == L'\0') {
                out += L"e";
            } else if (nxt == L'p' || nxt == L'b' || nxt == L'm' || nxt == L'f' || nxt == L'w' || nxt == L'v') {
                out += L"em";
            } else if (!IsPolishVowel(nxt)) {
                out += L"en";
            } else {
                out += L"e";
            }
            continue;
        }
        out.push_back(ch);
    }
    return out;
}

std::wstring NormalizeWord(std::wstring_view word) {
    std::wstring w = ToLowerPL(word);
    ReplaceAll(w, L"qu", L"k");
    ReplaceAll(w, L"q", L"k");
    ReplaceAll(w, L"v", L"w");
    ReplaceAll(w, L"x", L"ks");
    ReplaceAll(w, L"chrz", L"hsz");
    ReplaceAll(w, L"ó", L"u");
    ReplaceAll(w, L"ch", L"h");
    ReplaceAll(w, L"drż", L"drz");
    ReplaceAll(w, L"drz", L"d#");
    ReplaceAll(w, L"trz", L"t#");
    for (size_t i = 1; i + 1 < w.size(); ++i) {
        if (w[i] == L'r' && w[i + 1] == L'z' && IsVoicelessRzPrefix(w[i - 1])) {
            w.replace(i, 2, L"sz");
        }
    }
    ReplaceAll(w, L"rz", L"ż");
    ReplaceAll(w, L"d#", L"drz");
    ReplaceAll(w, L"t#", L"trz");
    ReplaceAll(w, L"au", L"ał");
    w = SoftenSequences(w);
    return NormalizeNasalVowels(w);
}

std::vector<std::wstring> SplitGraphemes(std::wstring_view word) {
    static const std::array<std::wstring, 9> kMulti = {
        L"szcz", L"źdź", L"drz", L"trz", L"dź", L"dż", L"dz", L"cz", L"sz",
    };

    std::vector<std::wstring> graphemes;
    size_t i = 0;
    while (i < word.size()) {
        bool matched = false;
        for (const auto& item : kMulti) {
            if (i + item.size() <= word.size() && word.substr(i, item.size()) == item) {
                graphemes.push_back(item);
                i += item.size();
                matched = true;
                break;
            }
        }
        if (!matched) {
            graphemes.emplace_back(word.substr(i, 1));
            ++i;
        }
    }

    std::vector<std::wstring> filtered;
    filtered.reserve(graphemes.size());
    for (size_t idx = 0; idx < graphemes.size(); ++idx) {
        const std::wstring& grapheme = graphemes[idx];
        const std::wstring prev = filtered.empty() ? L"" : filtered.back();
        const std::wstring nxt = (idx + 1 < graphemes.size()) ? graphemes[idx + 1] : L"";
        if (grapheme == L"i" && !prev.empty() &&
            (prev == L"ć" || prev == L"ś" || prev == L"ź" || prev == L"ń" || prev == L"dź") &&
            nxt.size() == 1 && IsPolishVowelWithI(nxt[0])) {
            continue;
        }
        filtered.push_back(grapheme);
    }
    return filtered;
}

std::vector<std::wstring> ApplyFinalDevoicing(std::vector<std::wstring> graphemes) {
    static const std::unordered_set<std::wstring> kVoicelessObstruents = {
        L"p", L"t", L"k", L"f", L"s", L"ś", L"sz", L"c", L"ć", L"cz", L"h"
    };
    static const std::unordered_set<std::wstring> kVoicedObstruents = {
        L"b", L"d", L"g", L"w", L"z", L"ź", L"ż", L"dz", L"dź", L"dż"
    };
    static const std::unordered_set<std::wstring> kVoicelessGraphemes = {
        L"p", L"t", L"k", L"f", L"s", L"ś", L"sz", L"c", L"ć", L"cz", L"h", L"ch", L"x"
    };
    static const std::unordered_map<std::wstring, std::wstring> kVoicedToVoiceless = {
        {L"b", L"p"}, {L"d", L"t"}, {L"g", L"k"}, {L"w", L"f"}, {L"z", L"s"},
        {L"ź", L"ś"}, {L"ż", L"sz"}, {L"dz", L"c"}, {L"dź", L"ć"}, {L"dż", L"cz"},
    };
    static const std::unordered_map<std::wstring, std::wstring> kVoicelessToVoiced = {
        {L"p", L"b"}, {L"t", L"d"}, {L"k", L"g"}, {L"f", L"w"}, {L"s", L"z"},
        {L"ś", L"ź"}, {L"sz", L"ż"}, {L"c", L"dz"}, {L"ć", L"dź"}, {L"cz", L"dż"},
    };

    for (int idx = static_cast<int>(graphemes.size()) - 2; idx >= 0; --idx) {
        const std::wstring current = graphemes[static_cast<size_t>(idx)];
        const std::wstring next = graphemes[static_cast<size_t>(idx + 1)];
        if (kVoicedObstruents.count(current) != 0 && kVoicelessObstruents.count(next) != 0) {
            graphemes[static_cast<size_t>(idx)] = kVoicedToVoiceless.at(current);
        } else if (kVoicelessObstruents.count(current) != 0 && kVoicedObstruents.count(next) != 0) {
            const auto it = kVoicelessToVoiced.find(current);
            if (it != kVoicelessToVoiced.end()) {
                graphemes[static_cast<size_t>(idx)] = it->second;
            }
        }
    }

    for (size_t idx = 0; idx < graphemes.size(); ++idx) {
        const std::wstring current = graphemes[idx];
        const std::wstring next = (idx + 1 < graphemes.size()) ? graphemes[idx + 1] : L"";
        const auto it = kVoicedToVoiceless.find(current);
        if (it != kVoicedToVoiceless.end() && (next.empty() || kVoicelessGraphemes.count(next) != 0)) {
            graphemes[idx] = it->second;
        }
    }

    return graphemes;
}

std::vector<SamUnit> MapGraphemes(const std::vector<std::wstring>& graphemes) {
    std::vector<SamUnit> mapped;
    mapped.reserve(graphemes.size() * 2);

    auto isLabial = [](const std::wstring& g) {
        return g == L"b" || g == L"p" || g == L"m" || g == L"f" || g == L"w";
    };

    for (size_t idx = 0; idx < graphemes.size(); ++idx) {
        const std::wstring& grapheme = graphemes[idx];
        const std::wstring nxt = (idx + 1 < graphemes.size()) ? graphemes[idx + 1] : L"";
        const std::wstring prev = (idx > 0) ? graphemes[idx - 1] : L"";
        const wchar_t nextChar = (nxt.size() == 1) ? nxt[0] : L'\0';
        const wchar_t prevChar = (prev.size() == 1) ? prev[0] : L'\0';

        if (grapheme == L"a") mapped.push_back({"AA", true});
        else if (grapheme == L"e") mapped.push_back({"EH", true});
        else if (grapheme == L"i") {
            if (nextChar != L'\0' && IsPolishVowel(nextChar) &&
                (prev.empty() || isLabial(prev) || (prevChar != L'\0' && IsPolishVowelWithI(prevChar)))) {
                mapped.push_back({"Y", false});
            } else if (nextChar != L'\0' && IsPolishVowel(nextChar)) {
                mapped.push_back({"IH", true});
            } else {
                mapped.push_back({"IY", true});
            }
        } else if (grapheme == L"y") mapped.push_back({"IH", true});
        else if (grapheme == L"o") mapped.push_back({"OH", true});
        else if (grapheme == L"u") mapped.push_back({"UW", true});
        else if (grapheme == L"p") mapped.push_back({"P", false});
        else if (grapheme == L"b") mapped.push_back({"B", false});
        else if (grapheme == L"t") mapped.push_back({"T", false});
        else if (grapheme == L"d") mapped.push_back({"D", false});
        else if (grapheme == L"k") mapped.push_back({"K", false});
        else if (grapheme == L"g") mapped.push_back({"G", false});
        else if (grapheme == L"f") mapped.push_back({"F", false});
        else if (grapheme == L"w") mapped.push_back({"V", false});
        else if (grapheme == L"m") mapped.push_back({"M", false});
        else if (grapheme == L"n") mapped.push_back({"N", false});
        else if (grapheme == L"ń") {
            if (nextChar != L'\0' && IsPolishVowelWithI(nextChar)) {
                mapped.push_back({"N", false});
                mapped.push_back({"Y", false});
            } else {
                mapped.push_back({"N", false});
            }
        } else if (grapheme == L"l") mapped.push_back({"L", false});
        else if (grapheme == L"ł") mapped.push_back({"W", false});
        else if (grapheme == L"r") mapped.push_back({"R", false});
        else if (grapheme == L"s") mapped.push_back({"S", false});
        else if (grapheme == L"z") mapped.push_back({"Z", false});
        else if (grapheme == L"ś") mapped.push_back({"SH", false});
        else if (grapheme == L"ź" || grapheme == L"ż") mapped.push_back({"ZH", false});
        else if (grapheme == L"h") mapped.push_back({"/H", false});
        else if (grapheme == L"j") {
            if (prevChar != L'\0' && nextChar != L'\0' && IsPolishVowelWithI(prevChar) && IsPolishVowelWithI(nextChar)) {
                mapped.push_back({"YX", false});
            } else {
                mapped.push_back({"Y", false});
            }
        } else if (grapheme == L"c") {
            mapped.push_back({"T", false});
            mapped.push_back({"S", false});
        } else if (grapheme == L"dz") {
            mapped.push_back({"D", false});
            mapped.push_back({"Z", false});
        } else if (grapheme == L"ć" || grapheme == L"cz") mapped.push_back({"CH", false});
        else if (grapheme == L"drz") {
            mapped.push_back({"D", false});
            mapped.push_back({"R", false});
            mapped.push_back({"ZH", false});
        } else if (grapheme == L"trz") {
            mapped.push_back({"T", false});
            mapped.push_back({"R", false});
            mapped.push_back({"SH", false});
        } else if (grapheme == L"szcz") {
            mapped.push_back({"SH", false});
            mapped.push_back({"CH", false});
        } else if (grapheme == L"źdź") {
            mapped.push_back({"ZH", false});
            mapped.push_back({"J", false});
        } else if (grapheme == L"dź" || grapheme == L"dż") mapped.push_back({"J", false});
        else if (grapheme == L"sz") mapped.push_back({"SH", false});
    }
    return mapped;
}

void ApplyStress(std::vector<SamUnit>& tokens, int stress) {
    std::vector<size_t> vowels;
    vowels.reserve(tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].isVowel) {
            vowels.push_back(i);
        }
    }
    if (vowels.size() < 2) {
        return;
    }
    const int clampedStress = std::max(1, std::min(8, stress));
    tokens[vowels[vowels.size() - 2]].phoneme.push_back(static_cast<char>('0' + clampedStress));
}

std::vector<SamUnit> WordToSamUnits(std::wstring_view word, int stress) {
    static const std::unordered_map<std::wstring, std::vector<SamUnit>> kSpecialWords = {
        {L"w", {{"V", false}}},
        {L"z", {{"Z", false}}},
        {L"i", {{"IY", true}}},
        {L"a", {{"AA", true}}},
        {L"o", {{"OH", true}}},
        {L"u", {{"UW", true}}},
    };

    const std::wstring lowered = ToLowerPL(word);
    const auto specialIt = kSpecialWords.find(lowered);
    if (specialIt != kSpecialWords.end()) {
        return specialIt->second;
    }

    const std::wstring normalized = NormalizeWord(lowered);
    auto graphemes = SplitGraphemes(normalized);
    graphemes = ApplyFinalDevoicing(std::move(graphemes));
    auto units = MapGraphemes(graphemes);
    ApplyStress(units, stress);
    return units;
}

std::vector<std::string> BuildPhoneStream(std::wstring_view text, int stress, bool keepPunctuation, bool keepWordBreaks) {
    std::vector<std::string> out;
    const std::wstring expanded = ExpandNumbers(text, NumberMode::Cardinal);
    const auto lexemes = TokenizeText(expanded);
    for (const auto& lexeme : lexemes) {
        if (lexeme.kind == Lexeme::Kind::Word) {
            const auto units = WordToSamUnits(lexeme.text, stress);
            for (const auto& unit : units) {
                out.push_back(unit.phoneme);
            }
            if (keepWordBreaks) {
                out.push_back("|");
            }
            continue;
        }
        if (!keepPunctuation || lexeme.text.empty()) {
            continue;
        }
        const wchar_t mark = lexeme.text[0];
        if (mark == L'.' || mark == L'?' || mark == L'!' || mark == L',' || mark == L';' || mark == L':' || mark == L'-') {
            out.emplace_back(1, static_cast<char>(mark));
        }
    }
    if (keepWordBreaks && !out.empty() && out.back() == "|") {
        out.pop_back();
    }
    return out;
}

std::string BasePhoneName(std::string phone) {
    while (!phone.empty() && phone.back() >= '0' && phone.back() <= '8') {
        phone.pop_back();
    }
    return phone;
}

int ExtractStress(std::string_view phone) {
    if (!phone.empty() && phone.back() >= '0' && phone.back() <= '8') {
        return phone.back() - '0';
    }
    return 0;
}

bool IsVowelPhone(std::string_view base) {
    return base == "AA" || base == "AE" || base == "AH" || base == "AO" || base == "AX" || base == "EH" ||
           base == "IH" || base == "IY" || base == "OH" || base == "OW" || base == "UH" || base == "UW" || base == "UX";
}

std::array<double, 3> GetFormants(std::string_view base) {
    static const std::unordered_map<std::string, std::array<double, 3>> kFormants = {
        {"AA", {720.0, 1100.0, 2450.0}},
        {"EH", {540.0, 1720.0, 2480.0}},
        {"IH", {370.0, 1600.0, 2400.0}},
        {"IY", {300.0, 2200.0, 2850.0}},
        {"OH", {560.0, 980.0, 2400.0}},
        {"UW", {340.0, 860.0, 2200.0}},
        {"Y", {300.0, 2150.0, 2750.0}},
        {"YX", {320.0, 1900.0, 2650.0}},
        {"M", {250.0, 1000.0, 2200.0}},
        {"N", {250.0, 1500.0, 2500.0}},
        {"L", {380.0, 1400.0, 2500.0}},
        {"W", {320.0, 900.0, 2200.0}},
        {"R", {430.0, 1280.0, 2500.0}},
    };
    const auto it = kFormants.find(std::string(base));
    return (it != kFormants.end()) ? it->second : std::array<double, 3>{0.0, 0.0, 0.0};
}

std::vector<float> GenRectPulse(double f1, double f2, double f3, int len) {
    std::vector<float> out;
    out.reserve(static_cast<size_t>(std::max(1, len)));
    for (int n = 0; n < std::max(1, len); ++n) {
        const double t = static_cast<double>(n) / static_cast<double>(kSampleRate);
        double v = std::sin(2.0 * 3.141592653589793 * f1 * t) * 0.82;
        v += std::sin(2.0 * 3.141592653589793 * f2 * t) * 0.48;
        v += std::sin(2.0 * 3.141592653589793 * f3 * t) * 0.26;
        v *= std::exp(-t * 300.0);
        out.push_back(static_cast<float>(v));
    }
    return out;
}

void AppendSilence(std::vector<float>& out, uint32_t samples) {
    out.insert(out.end(), samples, 0.0f);
}

void AppendNoise(std::vector<float>& out, uint32_t count, float amp, std::mt19937& rng, float sparse = 1.0f) {
    std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    for (uint32_t i = 0; i < count; ++i) {
        float sample = noise(rng);
        if (sparse < 1.0f && chance(rng) > sparse) {
            sample = 0.0f;
        }
        out.push_back(sample * amp);
    }
}

double ContourFactor(double pos, SamIntonationMode mode, double strength) {
    switch (mode) {
    case SamIntonationMode::Flat:
        return 1.0;
    case SamIntonationMode::Question:
        return pos < 0.68 ? (0.98 + 0.05 * pos * strength) : (1.0 + 0.40 * ((pos - 0.68) / 0.32) * strength);
    case SamIntonationMode::Exclamation:
        return 1.15 + 0.05 * std::sin(pos * 6.28318) - 0.20 * pos * strength;
    case SamIntonationMode::Statement:
        return 1.03 - 0.12 * pos * strength;
    case SamIntonationMode::Auto:
    default:
        return 1.02 - 0.10 * pos * strength;
    }
}

SamIntonationMode ResolveMode(SamIntonationMode configured, SentenceMark mark) {
    if (configured != SamIntonationMode::Auto) {
        return configured;
    }
    switch (mark) {
    case SentenceMark::Question:
        return SamIntonationMode::Question;
    case SentenceMark::Exclamation:
        return SamIntonationMode::Exclamation;
    case SentenceMark::Statement:
    case SentenceMark::Comma:
    case SentenceMark::Semicolon:
    case SentenceMark::Colon:
    default:
        return SamIntonationMode::Statement;
    }
}

double PhoneDuration(std::string_view base, int stress) {
    double seconds = 0.055;
    if (IsVowelPhone(base)) {
        seconds = 0.085;
    } else if (base == "M" || base == "N" || base == "L" || base == "W" || base == "R" || base == "Y" || base == "YX") {
        seconds = 0.062;
    } else if (base == "P" || base == "B" || base == "T" || base == "D" || base == "K" || base == "G") {
        seconds = 0.040;
    } else if (base == "CH" || base == "J") {
        seconds = 0.055;
    } else if (base == "S" || base == "SH" || base == "F" || base == "/H" || base == "Z" || base == "ZH" || base == "V") {
        seconds = 0.050;
    }
    if (stress > 0 && IsVowelPhone(base)) {
        seconds *= 1.22;
    }
    return seconds;
}

void RenderPhone(
    std::vector<float>& output,
    std::string_view phone,
    double localPitch,
    double rateMult,
    bool quantizeToC64,
    std::array<double, 3>& currentFormants,
    std::mt19937& rng
) {
    const std::string base = BasePhoneName(std::string(phone));
    const int stress = ExtractStress(phone);
    const double duration = PhoneDuration(base, stress) * rateMult;
    const float unvoicedNoiseGain = quantizeToC64 ? 0.15f : 0.11f;
    const float voicedNoiseGain = quantizeToC64 ? 0.07f : 0.05f;
    const double formantSlew = quantizeToC64 ? 0.30 : 0.40;

    if (base == "R") {
        const int taps = 3;
        for (int i = 0; i < taps; ++i) {
            auto pulse = GenRectPulse(430.0, 1280.0, 2500.0, static_cast<int>(kSampleRate / std::max(24.0, localPitch * 2.8)));
            output.insert(output.end(), pulse.begin(), pulse.end());
            AppendSilence(output, static_cast<uint32_t>(kSampleRate * 0.004));
        }
        return;
    }

    const auto formants = GetFormants(base);
    if (formants[0] > 0.0) {
        const int pulseLen = std::max(1, static_cast<int>(kSampleRate / std::max(30.0, localPitch)));
        const int cycles = std::max(1, static_cast<int>((kSampleRate * duration) / pulseLen));
        for (int c = 0; c < cycles; ++c) {
            for (int i = 0; i < 3; ++i) {
                currentFormants[i] += (formants[i] - currentFormants[i]) * formantSlew;
            }
            auto pulse = GenRectPulse(currentFormants[0], currentFormants[1], currentFormants[2], pulseLen);
            const float gain = (stress > 0 && IsVowelPhone(base)) ? 1.08f : 1.0f;
            for (float sample : pulse) {
                output.push_back(sample * gain);
            }
        }
        return;
    }

    if (base == "S" || base == "SH" || base == "F" || base == "/H") {
        float amp = unvoicedNoiseGain;
        float sparse = 0.22f;
        if (base == "SH") {
            amp *= 1.32f;
            sparse = 0.36f;
        } else if (base == "/H") {
            amp *= 0.85f;
            sparse = 0.18f;
        }
        AppendNoise(output, static_cast<uint32_t>(kSampleRate * duration), amp, rng, sparse);
        return;
    }

    if (base == "Z" || base == "ZH" || base == "V") {
        auto basePulse = GenRectPulse(380.0, 1380.0, 2280.0, static_cast<int>(kSampleRate / std::max(30.0, localPitch)));
        const int cycles = std::max(1, static_cast<int>((kSampleRate * duration) / std::max<size_t>(1, basePulse.size())));
        std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
        std::uniform_real_distribution<float> chance(0.0f, 1.0f);
        float sparse = (base == "ZH") ? 0.36f : 0.28f;
        float voiceMix = (base == "V") ? 0.76f : 0.68f;
        float noiseMix = (base == "ZH") ? voicedNoiseGain * 1.45f : voicedNoiseGain * 1.10f;
        for (int c = 0; c < cycles; ++c) {
            for (float sample : basePulse) {
                float n = noise(rng);
                if (chance(rng) > sparse) {
                    n = 0.0f;
                }
                output.push_back(sample * voiceMix + n * noiseMix);
            }
        }
        return;
    }

    if (base == "P" || base == "B" || base == "T" || base == "D" || base == "K" || base == "G") {
        AppendSilence(output, static_cast<uint32_t>(kSampleRate * 0.007));
        float amp = 0.34f;
        uint32_t burst = static_cast<uint32_t>(kSampleRate * 0.012);
        if (base == "T") {
            amp = 0.52f;
            burst = static_cast<uint32_t>(kSampleRate * 0.016);
        } else if (base == "K") {
            amp = 0.55f;
            burst = static_cast<uint32_t>(kSampleRate * 0.017);
        } else if (base == "B" || base == "D" || base == "G") {
            amp *= 0.88f;
        }
        AppendNoise(output, burst, amp, rng, 0.35f);
        if (base == "B" || base == "D" || base == "G") {
            auto voicePulse = GenRectPulse(320.0, 1100.0, 2200.0, static_cast<int>(kSampleRate / std::max(30.0, localPitch)));
            output.insert(output.end(), voicePulse.begin(), voicePulse.end());
        }
        return;
    }

    if (base == "CH" || base == "J") {
        AppendSilence(output, static_cast<uint32_t>(kSampleRate * 0.008));
        const float amp = (base == "J") ? unvoicedNoiseGain * 1.28f : unvoicedNoiseGain * 1.42f;
        AppendNoise(output, static_cast<uint32_t>(kSampleRate * duration), amp, rng, 0.24f);
        return;
    }

    AppendSilence(output, static_cast<uint32_t>(kSampleRate * std::max(0.01, duration * 0.6)));
}

char SamPunctuationSuffix(SentenceMark mark, SamIntonationMode mode) {
    if (mode == SamIntonationMode::Question) {
        return '?';
    }
    switch (mark) {
    case SentenceMark::Question:
        return '?';
    case SentenceMark::Comma:
    case SentenceMark::Semicolon:
    case SentenceMark::Colon:
        return ',';
    case SentenceMark::Exclamation:
    case SentenceMark::Statement:
    default:
        return '.';
    }
}

std::string JoinPhones(const std::vector<std::string>& phones) {
    std::string out;
    for (const auto& phone : phones) {
        if (phone == "|" || phone.empty()) {
            continue;
        }
        const bool punctuation = phone.size() == 1 && std::ispunct(static_cast<unsigned char>(phone[0])) != 0;
        if (!out.empty() && !punctuation) {
            out.push_back(' ');
        }
        out += phone;
    }
    return out;
}

std::vector<std::wstring> SplitChunkForSam(std::wstring_view text, int stress, size_t maxPhonemeLen) {
    std::vector<std::wstring> segments;
    std::wstring current;
    std::wstring word;

    auto flushWord = [&]() {
        if (word.empty()) {
            return;
        }
        const std::wstring candidate = current.empty() ? word : (current + L" " + word);
        const size_t candidateLen = JoinPhones(BuildPhoneStream(candidate, stress, false, false)).size();
        if (!current.empty() && candidateLen > maxPhonemeLen) {
            segments.push_back(current);
            current = word;
        } else {
            current = candidate;
        }
        word.clear();
    };

    for (wchar_t ch : text) {
        if (std::iswspace(ch)) {
            flushWord();
        } else {
            word.push_back(ch);
        }
    }
    flushWord();
    if (!current.empty()) {
        segments.push_back(current);
    }
    if (segments.empty() && !text.empty()) {
        segments.push_back(std::wstring(text));
    }
    return segments;
}

VoiceParams ScaleIntonation(const VoiceParams& voice, SentenceMark mark, int strength) {
    VoiceParams out = voice;
    const double factor = std::clamp(static_cast<double>(strength) / 100.0, 0.35, 2.0);
    auto applyDelta = [factor](int base, int delta) {
        const int scaled = static_cast<int>(std::lround(static_cast<double>(delta) * factor));
        return std::max(0, std::min(255, base + scaled));
    };

    switch (mark) {
    case SentenceMark::Question:
        out.pitch = applyDelta(out.pitch, 10);
        out.speed = applyDelta(out.speed, -4);
        out.mouth = applyDelta(out.mouth, 4);
        break;
    case SentenceMark::Exclamation:
        out.pitch = applyDelta(out.pitch, 14);
        out.speed = applyDelta(out.speed, 3);
        out.mouth = applyDelta(out.mouth, 8);
        break;
    case SentenceMark::Comma:
    case SentenceMark::Semicolon:
    case SentenceMark::Colon:
        out.pitch = applyDelta(out.pitch, -2);
        out.speed = applyDelta(out.speed, -2);
        break;
    case SentenceMark::Statement:
    default:
        break;
    }
    return out;
}

std::vector<uint8_t> RenderSamPhonemeSegment(
    const std::string& phonemes,
    const VoiceParams& voice,
    bool singMode
) {
    static std::mutex samMutex;
    std::lock_guard<std::mutex> lock(samMutex);

    std::string input = phonemes;
    input.push_back(static_cast<char>(0x9b));
    input.push_back('\0');

    SetSpeed(static_cast<unsigned char>(std::max(0, std::min(255, voice.speed))));
    SetPitch(static_cast<unsigned char>(std::max(0, std::min(255, voice.pitch))));
    SetMouth(static_cast<unsigned char>(std::max(0, std::min(255, voice.mouth))));
    SetThroat(static_cast<unsigned char>(std::max(0, std::min(255, voice.throat))));
    if (singMode) {
        EnableSingmode();
    }
    SetInput(input.data());

    if (!SAMMain()) {
        return {};
    }

    const int rawLength = GetBufferLength();
    const int finalLength = std::max(0, rawLength / 50);
    const unsigned char* buffer = reinterpret_cast<const unsigned char*>(GetBuffer());

    std::vector<uint8_t> pcm;
    pcm.reserve(static_cast<size_t>(finalLength) * sizeof(int16_t));
    for (int i = 0; i < finalLength; ++i) {
        const int centered = static_cast<int>(buffer[i]) - 128;
        const int16_t sample = static_cast<int16_t>(std::clamp(centered * 256, -32768, 32767));
        const uint8_t* raw = reinterpret_cast<const uint8_t*>(&sample);
        pcm.push_back(raw[0]);
        pcm.push_back(raw[1]);
    }
    return pcm;
}

}  // namespace

std::vector<std::string> TextToSamPhonemes(std::wstring_view text, int stress) {
    std::vector<std::string> out = BuildPhoneStream(text, stress, true, false);
    if (!out.empty() && out.back() == "|") {
        out.pop_back();
    }
    return out;
}

std::wstring DebugSamPhonemes(std::wstring_view text, int stress) {
    const auto phones = TextToSamPhonemes(text, stress);
    std::wstring out;
    for (const auto& phone : phones) {
        const bool punctuation = phone.size() == 1 && std::ispunct(static_cast<unsigned char>(phone[0])) != 0;
        if (!out.empty() && !punctuation) {
            out.push_back(L' ');
        }
        for (char ch : phone) {
            out.push_back(static_cast<wchar_t>(ch));
        }
    }
    return out;
}

std::vector<uint8_t> SynthesizePolishSamLike(std::wstring_view text, const SamVoiceSettings& settings) {
    std::vector<uint8_t> pcm;
    const auto chunks = SplitProsodyChunks(ExpandNumbers(text, NumberMode::Cardinal));
    for (const auto& chunk : chunks) {
        const auto textSegments = SplitChunkForSam(chunk.text, settings.stress, 96);
        for (size_t i = 0; i < textSegments.size(); ++i) {
            const bool lastSegment = (i + 1 == textSegments.size());
            const SentenceMark segMark = lastSegment ? chunk.mark : SentenceMark::Comma;
            const auto phones = BuildPhoneStream(textSegments[i], settings.stress, false, false);
            if (phones.empty()) {
                continue;
            }

            std::string phonemeInput = JoinPhones(phones);
            const SamIntonationMode mode = ResolveMode(settings.intonationMode, segMark);
            phonemeInput.push_back(SamPunctuationSuffix(segMark, mode));

            VoiceParams voice = settings.voice;
            if (mode == SamIntonationMode::Question) {
                voice = ScaleIntonation(voice, SentenceMark::Question, settings.intonationStrength);
            } else if (mode == SamIntonationMode::Exclamation) {
                voice = ScaleIntonation(voice, SentenceMark::Exclamation, settings.intonationStrength);
            } else {
                voice = ScaleIntonation(voice, segMark, settings.intonationStrength);
            }

            auto chunkPcm = RenderSamPhonemeSegment(phonemeInput, voice, false);
            pcm.insert(pcm.end(), chunkPcm.begin(), chunkPcm.end());
        }
    }

    const int volume = std::clamp(settings.volume, 0, 100);
    if (volume < 100) {
        const float gain = static_cast<float>(volume) / 100.0f;
        for (size_t i = 0; i + 1 < pcm.size(); i += 2) {
            int16_t sample = static_cast<int16_t>(static_cast<uint16_t>(pcm[i]) | (static_cast<uint16_t>(pcm[i + 1]) << 8));
            const int scaled = static_cast<int>(std::lround(static_cast<float>(sample) * gain));
            const int16_t out = static_cast<int16_t>(std::clamp(scaled, -32768, 32767));
            pcm[i] = static_cast<uint8_t>(out & 0xff);
            pcm[i + 1] = static_cast<uint8_t>((static_cast<uint16_t>(out) >> 8) & 0xff);
        }
    }

    return pcm;
}

}  // namespace blackbox
