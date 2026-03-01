#include <windows.h>
#include <sapi.h>
#include <sapiddk.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <new>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

const CLSID CLSID_BlackBoxSapi5 =
{ 0x9b5d8344, 0x4d5e, 0x46a2, { 0x80, 0xd5, 0x2d, 0x83, 0xcc, 0x6b, 0xc2, 0x7d } };

constexpr wchar_t kEngineName[] = L"BlackBox V8 SAPI5 Engine";
constexpr uint32_t kSampleRate = 22050;

HMODULE g_module = nullptr;
std::atomic<ULONG> g_objects{0};
std::atomic<ULONG> g_locks{0};

enum class IntonationMode { Auto, Flat, Statement, Question, Exclamation };
enum class TokenType { Phone, PauseShort, PauseLong };
enum class VoiceFlavor { C64, Clear };
enum class NumberMode { Cardinal, Digits };
enum class SymbolLevel { None, Some, Most, All };

struct Token { TokenType type; std::wstring p; };
struct SentenceChunk { std::wstring text; wchar_t mark; };

HRESULT WriteRegString(HKEY root, const wchar_t* subKey, const wchar_t* name, const wchar_t* value) {
    HKEY key = nullptr;
    LONG rc = RegCreateKeyExW(root, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (rc != ERROR_SUCCESS) return HRESULT_FROM_WIN32(rc);
    const DWORD cb = static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t));
    rc = RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value), cb);
    RegCloseKey(key);
    return (rc == ERROR_SUCCESS) ? S_OK : HRESULT_FROM_WIN32(rc);
}

HRESULT DeleteRegTree(HKEY root, const wchar_t* subKey) {
    LONG rc = RegDeleteTreeW(root, subKey);
    if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND) return S_OK;
    return (rc == ERROR_SUCCESS) ? S_OK : HRESULT_FROM_WIN32(rc);
}

wchar_t LowerPL(wchar_t ch) {
    switch (ch) {
    case L'Ą': return L'ą'; case L'Ć': return L'ć'; case L'Ę': return L'ę'; case L'Ł': return L'ł';
    case L'Ń': return L'ń'; case L'Ó': return L'ó'; case L'Ś': return L'ś'; case L'Ź': return L'ź'; case L'Ż': return L'ż';
    default: return static_cast<wchar_t>(towlower(ch));
    }
}

std::wstring ToLowerPL(const std::wstring& s) { std::wstring o; o.reserve(s.size()); for (auto c : s) o.push_back(LowerPL(c)); return o; }
bool IsWhitespace(wchar_t ch) { return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n'; }
bool IsAsciiAlphaNum(wchar_t ch) { return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9'); }
int ClampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

void ReplaceAll(std::wstring& s, const std::wstring& from, const std::wstring& to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::wstring::npos) { s.replace(pos, from.size(), to); pos += to.size(); }
}

std::wstring JoinSpeakText(const SPVTEXTFRAG* fragList) {
    std::wstring out;
    for (auto f = fragList; f; f = f->pNext) {
        if (f->pTextStart && f->ulTextLen > 0 && f->State.eAction != SPVA_Silence) {
            out.append(f->pTextStart, f->ulTextLen);
            out.push_back(L' ');
        } else if (f->State.eAction == SPVA_Silence) {
            out.push_back(L' ');
        }
    }
    return out;
}

int RateAdjustToPercent(long rateAdjust) { return ClampInt(50 + static_cast<int>(rateAdjust) * 4, 0, 100); }
int PitchAdjustToPercent(const SPVTEXTFRAG* fragList) {
    long sum = 0, count = 0;
    for (auto f = fragList; f; f = f->pNext) {
        if ((f->State.eAction == SPVA_Speak || f->State.eAction == SPVA_Pronounce || f->State.eAction == SPVA_SpellOut) && f->pTextStart && f->ulTextLen > 0) {
            sum += f->State.PitchAdj.MiddleAdj; ++count;
        }
    }
    return ClampInt(50 + static_cast<int>((count > 0 ? sum / count : 0)) * 3, 0, 100);
}
int EmphasisAdjust(const SPVTEXTFRAG* fragList) {
    long sum = 0, count = 0;
    for (auto f = fragList; f; f = f->pNext) {
        if ((f->State.eAction == SPVA_Speak || f->State.eAction == SPVA_Pronounce || f->State.eAction == SPVA_SpellOut) && f->pTextStart && f->ulTextLen > 0) {
            sum += f->State.EmphAdj; ++count;
        }
    }
    return count > 0 ? static_cast<int>(sum / count) : 0;
}
std::wstring TripletToWords(int n) {
    static const std::array<const wchar_t*, 10> H = { L"",L"sto",L"dwieście",L"trzysta",L"czterysta",L"pięćset",L"sześćset",L"siedemset",L"osiemset",L"dziewięćset" };
    static const std::array<const wchar_t*, 10> T = { L"",L"dziesięć",L"dwadzieścia",L"trzydzieści",L"czterdzieści",L"pięćdziesiąt",L"sześćdziesiąt",L"siedemdziesiąt",L"osiemdziesiąt",L"dziewięćdziesiąt" };
    static const std::array<const wchar_t*, 10> U = { L"zero",L"jeden",L"dwa",L"trzy",L"cztery",L"pięć",L"sześć",L"siedem",L"osiem",L"dziewięć" };
    static const std::array<const wchar_t*, 10> N = { L"dziesięć",L"jedenaście",L"dwanaście",L"trzynaście",L"czternaście",L"piętnaście",L"szesnaście",L"siedemnaście",L"osiemnaście",L"dziewiętnaście" };
    int h = n / 100, t = (n / 10) % 10, u = n % 10;
    std::wstring out;
    if (h > 0) out += H[h];
    if (t == 1) { if (!out.empty()) out += L' '; out += N[u]; }
    else {
        if (t > 0) { if (!out.empty()) out += L' '; out += T[t]; }
        if (u > 0) { if (!out.empty()) out += L' '; out += U[u]; }
    }
    return out;
}

const wchar_t* PluralForm(int n, const wchar_t* one, const wchar_t* few, const wchar_t* many) {
    if (n == 1) return one;
    int last2 = n % 100, last = n % 10;
    if (last2 > 10 && last2 < 20) return many;
    if (last >= 2 && last <= 4) return few;
    return many;
}

std::wstring IntToPolishWords(long long n) {
    if (n == 0) return L"zero";
    if (n < 0) return L"minus " + IntToPolishWords(-n);
    std::vector<std::wstring> parts;
    int chunk = static_cast<int>(n % 1000);
    if (chunk > 0) parts.push_back(TripletToWords(chunk));
    n /= 1000;
    int g = 1;
    while (n > 0) {
        chunk = static_cast<int>(n % 1000);
        if (chunk > 0) {
            std::wstring w = TripletToWords(chunk);
            if (g == 1) w = (chunk == 1) ? L"tysiąc" : w + L" " + PluralForm(chunk, L"tysiąc", L"tysiące", L"tysięcy");
            else if (g == 2) w += L" " + std::wstring(PluralForm(chunk, L"milion", L"miliony", L"milionów"));
            else if (g == 3) w += L" " + std::wstring(PluralForm(chunk, L"miliard", L"miliardy", L"miliardów"));
            else if (g == 4) w += L" " + std::wstring(PluralForm(chunk, L"bilion", L"biliony", L"bilionów"));
            parts.push_back(w);
        }
        n /= 1000; ++g;
    }
    std::wstring out;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) { if (!out.empty()) out += L' '; out += *it; }
    return out;
}

std::wstring DigitsToPolishWords(const std::wstring& digits) {
    static const std::array<const wchar_t*, 10> U = { L"zero",L"jeden",L"dwa",L"trzy",L"cztery",L"pięć",L"sześć",L"siedem",L"osiem",L"dziewięć" };
    std::wstring out;
    for (wchar_t ch : digits) {
        if (ch == L'-') {
            if (!out.empty()) out += L' ';
            out += L"minus";
            continue;
        }
        if (ch >= L'0' && ch <= L'9') {
            if (!out.empty()) out += L' ';
            out += U[static_cast<size_t>(ch - L'0')];
        }
    }
    return out;
}

std::wstring ExpandNumbers(const std::wstring& in, NumberMode numberMode) {
    std::wstring out;
    size_t i = 0;
    while (i < in.size()) {
        wchar_t ch = in[i];
        bool minus = (ch == L'-' && i + 1 < in.size() && iswdigit(in[i + 1]));
        if (iswdigit(ch) || minus) {
            size_t start = i;
            size_t j = i + (minus ? 1 : 0);
            while (j < in.size() && iswdigit(in[j])) ++j;
            wchar_t left = (start > 0) ? in[start - 1] : L' ';
            wchar_t right = (j < in.size()) ? in[j] : L' ';
            if (!IsAsciiAlphaNum(left) && !IsAsciiAlphaNum(right)) {
                try {
                    const std::wstring raw = in.substr(start, j - start);
                    out += L' ';
                    out += (numberMode == NumberMode::Digits) ? DigitsToPolishWords(raw) : IntToPolishWords(std::stoll(raw));
                    out += L' ';
                    i = j;
                    continue;
                } catch (...) {}
            }
        }
        out.push_back(ch);
        ++i;
    }
    return out;
}

std::wstring NormalizePolishText(const std::wstring& in) {
    std::wstring s = ToLowerPL(in);
    ReplaceAll(s, L"ó", L"u"); ReplaceAll(s, L"ch", L"h"); ReplaceAll(s, L"rz", L"ż");
    auto soften = [&s](const std::wstring& pat, const std::wstring& repl) {
        static const std::array<wchar_t, 8> V = { L'a',L'e',L'i',L'o',L'u',L'y',L'ą',L'ę' };
        for (auto v : V) ReplaceAll(s, pat + std::wstring(1, v), repl + std::wstring(1, v));
        ReplaceAll(s, pat, repl + L"i");
    };
    soften(L"ci", L"ć"); soften(L"si", L"ś"); soften(L"zi", L"ź"); soften(L"ni", L"ń"); soften(L"dzi", L"dź");
    std::wstring nasal; nasal.reserve(s.size() + 8);
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t ch = s[i], nxt = (i + 1 < s.size()) ? s[i + 1] : L' ';
        if (ch == L'ę') { if (nxt == L' ') nasal += L"e"; else if (nxt == L'p' || nxt == L'b') nasal += L"em"; else if (nxt == L't' || nxt == L'd' || nxt == L'c' || nxt == L'z') nasal += L"en"; else nasal += L"ę"; }
        else if (ch == L'ą') { if (nxt == L' ') nasal += L"oł"; else if (nxt == L'p' || nxt == L'b') nasal += L"om"; else if (nxt == L't' || nxt == L'd' || nxt == L'c' || nxt == L'z') nasal += L"on"; else nasal += L"ą"; }
        else nasal.push_back(ch);
    }
    return nasal;
}

std::vector<SentenceChunk> SplitSentences(const std::wstring& text) {
    std::vector<SentenceChunk> out;
    std::wstring cur;
    for (wchar_t ch : text) {
        cur.push_back(ch);
        if (ch == L'.' || ch == L'?' || ch == L'!') { out.push_back({cur, ch}); cur.clear(); }
    }
    if (!cur.empty()) out.push_back({cur, L'.'});
    if (out.empty()) out.push_back({text, L'.'});
    return out;
}
std::array<double, 3> GetFormants(const std::wstring& t) {
    static const std::unordered_map<std::wstring, std::array<double, 3>> M = {
        {L"a", {750,1200,2400}}, {L"e", {500,1700,2500}}, {L"i", {280,2300,2900}}, {L"o", {550,1050,2400}}, {L"u", {350,800,2200}}, {L"y", {350,1450,2350}},
        {L"m", {250,1000,2200}}, {L"n", {250,1500,2500}}, {L"ń", {250,2100,2800}}, {L"l", {400,1500,2500}}, {L"ł", {300,900,2200}}, {L"j", {280,2250,2890}},
        {L"ę", {520,1800,2700}}, {L"ą", {550,1000,2400}},
        {L"ł", {300,1200,2300}}
    };
    auto it = M.find(t);
    return it != M.end() ? it->second : std::array<double, 3>{0,0,0};
}

std::vector<float> GenRectPulse(double f1, double f2, double f3, int len) {
    std::vector<float> out; out.reserve(static_cast<size_t>(std::max(1, len)));
    for (int n = 0; n < std::max(1, len); ++n) {
        double tt = static_cast<double>(n) / kSampleRate;
        double v = std::sin(2.0 * 3.141592653589793 * f1 * tt) * 0.82 + std::sin(2.0 * 3.141592653589793 * f2 * tt) * 0.5 + std::sin(2.0 * 3.141592653589793 * f3 * tt) * 0.28;
        v *= std::exp(-tt * 330.0);
        out.push_back(static_cast<float>(v));
    }
    return out;
}

void AppendSilence(std::vector<float>& out, uint32_t samples) { out.insert(out.end(), samples, 0.0f); }
void AppendNoise(std::vector<float>& out, uint32_t count, float amp, std::mt19937& rng, float sparse = 1.0f) {
    std::uniform_real_distribution<float> u(-1.0f, 1.0f), c(0.0f, 1.0f);
    for (uint32_t i = 0; i < count; ++i) { float v = u(rng); if (sparse < 1.0f && c(rng) > sparse) v = 0.0f; out.push_back(v * amp); }
}

std::vector<Token> TokenizeToPhonemes(const std::wstring& sentence, SymbolLevel symbolLevel) {
    std::vector<Token> tokens;
    std::wstring s = NormalizePolishText(sentence);
    for (size_t i = 0; i < s.size();) {
        wchar_t ch = s[i], n1 = (i + 1 < s.size()) ? s[i + 1] : L'\0', n2 = (i + 2 < s.size()) ? s[i + 2] : L'\0';
        if (IsWhitespace(ch)) { tokens.push_back({TokenType::PauseShort, L""}); ++i; continue; }
        if (ch == L',' || ch == L':' || ch == L';') {
            if (symbolLevel != SymbolLevel::None) tokens.push_back({TokenType::PauseShort, L""});
            ++i; continue;
        }
        if (ch == L'.' || ch == L'!' || ch == L'?') {
            if (symbolLevel != SymbolLevel::None) tokens.push_back({TokenType::PauseLong, L""});
            ++i; continue;
        }
        if (ch == L's' && n1 == L'z') { tokens.push_back({TokenType::Phone, L"sz"}); i += 2; continue; }
        if (ch == L'c' && n1 == L'z') { tokens.push_back({TokenType::Phone, L"cz"}); i += 2; continue; }
        if (ch == L'd' && n1 == L'z' && (n2 == L'i' || n2 == L'ź')) { tokens.push_back({TokenType::Phone, L"dź"}); i += 3; continue; }
        if (ch == L'd' && n1 == L'z' && n2 == L'ż') { tokens.push_back({TokenType::Phone, L"dż"}); i += 3; continue; }
        if (ch == L'd' && n1 == L'z') { tokens.push_back({TokenType::Phone, L"dz"}); i += 2; continue; }
        bool letter = (ch >= L'a' && ch <= L'z') || ch == L'ą' || ch == L'ć' || ch == L'ę' || ch == L'ł' || ch == L'ń' || ch == L'ś' || ch == L'ź' || ch == L'ż';
        if (letter) tokens.push_back({TokenType::Phone, std::wstring(1, ch)});
        else if (symbolLevel == SymbolLevel::All && (ch == L'@' || ch == L'#' || ch == L'%' || ch == L'+' || ch == L'-' || ch == L'=' || ch == L'/')) tokens.push_back({TokenType::PauseShort, L""});
        ++i;
    }
    const std::unordered_map<std::wstring, std::wstring> D = { {L"b",L"p"},{L"d",L"t"},{L"g",L"k"},{L"z",L"s"},{L"ż",L"sz"},{L"ź",L"ś"},{L"w",L"f"},{L"dz",L"c"},{L"dź",L"ć"},{L"dż",L"cz"} };
    const std::unordered_map<std::wstring, bool> V = { {L"p",1},{L"t",1},{L"k",1},{L"s",1},{L"c",1},{L"ś",1},{L"f",1},{L"h",1},{L"sz",1},{L"cz",1},{L"ć",1} };
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].type != TokenType::Phone) continue;
        auto d = D.find(tokens[i].p); if (d == D.end()) continue;
        size_t j = i + 1; while (j < tokens.size() && tokens[j].type == TokenType::PauseShort) ++j;
        bool devo = false;
        if (j >= tokens.size() || tokens[j].type == TokenType::PauseLong) devo = true;
        else if (tokens[j].type == TokenType::Phone && V.find(tokens[j].p) != V.end()) devo = true;
        if (devo) tokens[i].p = d->second;
    }
    return tokens;
}

double ContourFactor(double pos, IntonationMode mode, double str) {
    switch (mode) {
    case IntonationMode::Flat: return 1.0;
    case IntonationMode::Question: return pos < 0.65 ? (0.97 + 0.06 * pos * str) : (1.0 + 0.45 * ((pos - 0.65) / 0.35) * str);
    case IntonationMode::Exclamation: return 1.17 + 0.06 * std::sin(pos * 6.28318) - 0.24 * pos * str;
    case IntonationMode::Statement: return 1.04 - 0.14 * pos * str;
    case IntonationMode::Auto:
    default: return 1.02 - 0.10 * pos * str;
    }
}

IntonationMode ParseIntonationMode(const wchar_t* raw) {
    if (!raw) return IntonationMode::Auto;
    std::wstring v = ToLowerPL(raw);
    if (v == L"flat") return IntonationMode::Flat;
    if (v == L"statement") return IntonationMode::Statement;
    if (v == L"question") return IntonationMode::Question;
    if (v == L"exclamation") return IntonationMode::Exclamation;
    return IntonationMode::Auto;
}

IntonationMode ResolveSentenceMode(IntonationMode configured, wchar_t mark, int emphAdj) {
    if (configured != IntonationMode::Auto) return configured;
    if (mark == L'?') return IntonationMode::Question;
    if (mark == L'!' || emphAdj > 6) return IntonationMode::Exclamation;
    return IntonationMode::Statement;
}
VoiceFlavor ParseVoiceFlavor(const wchar_t* raw) {
    if (!raw) return VoiceFlavor::C64;
    std::wstring v = ToLowerPL(raw);
    if (v == L"clear" || v == L"clean" || v == L"hifi") return VoiceFlavor::Clear;
    return VoiceFlavor::C64;
}

NumberMode ParseNumberMode(const wchar_t* raw) {
    if (!raw) return NumberMode::Cardinal;
    std::wstring v = ToLowerPL(raw);
    if (v == L"digits" || v == L"digit") return NumberMode::Digits;
    return NumberMode::Cardinal;
}

SymbolLevel ParseSymbolLevel(const wchar_t* raw) {
    if (!raw) return SymbolLevel::Most;
    std::wstring v = ToLowerPL(raw);
    if (v == L"none" || v == L"brak") return SymbolLevel::None;
    if (v == L"some" || v == L"czesc" || v == L"część") return SymbolLevel::Some;
    if (v == L"all" || v == L"wszystkie") return SymbolLevel::All;
    return SymbolLevel::Most;
}

bool IsVowelPhone(const std::wstring& p) {
    return p == L"a" || p == L"e" || p == L"i" || p == L"o" || p == L"u" || p == L"y" || p == L"ą" || p == L"ę";
}

std::vector<uint8_t> NativeSynthesize(const std::wstring& text, int ratePercent, int pitchPercent, int volumePercent, IntonationMode configuredMode, int intonationStrength, SymbolLevel symbolLevel, NumberMode numberMode, int emphAdj, VoiceFlavor flavor) {
    std::wstring expanded = ExpandNumbers(text, numberMode);
    auto chunks = SplitSentences(expanded);

    double rateMult = 1.0 / (0.35 + (static_cast<double>(ratePercent) / 100.0) * 2.30);
    double p = std::clamp(static_cast<double>(pitchPercent) / 100.0, 0.0, 1.0);
    // Keep default pitch in a medium range; apps can still override via pitch/rate.
    double basePitchHz = 40.0 + 250.0 * std::pow(p, 1.30);
    double volumeMult = std::clamp(static_cast<double>(volumePercent) / 100.0, 0.0, 2.0);
    double contourStrength = std::clamp(static_cast<double>(intonationStrength) / 100.0, 0.25, 2.0);
    const double formantSlew = (flavor == VoiceFlavor::Clear) ? 0.38 : 0.30;
    const float unvoicedNoiseGain = (flavor == VoiceFlavor::Clear) ? 0.11f : 0.15f;
    const float voicedNoiseGain = (flavor == VoiceFlavor::Clear) ? 0.05f : 0.07f;
    const bool quantizeToC64 = (flavor == VoiceFlavor::C64);

    uint32_t seed = 0;
    for (wchar_t ch : text) seed = (seed * 1315423911u) ^ static_cast<uint32_t>(ch);
    seed ^= static_cast<uint32_t>(ratePercent * 31 + pitchPercent * 17 + volumePercent * 13 + intonationStrength * 7);
    std::mt19937 rng(seed);

    std::vector<float> output;
    output.reserve(text.size() * 2000);
    std::array<double, 3> curF = {500,1000,2000};

    for (const auto& chunk : chunks) {
        auto tokens = TokenizeToPhonemes(chunk.text, symbolLevel);
        if (tokens.empty()) continue;

        size_t speechCount = 0;
        for (const auto& t : tokens) if (t.type == TokenType::Phone) ++speechCount;
        speechCount = std::max<size_t>(1, speechCount);
        IntonationMode sentenceMode = ResolveSentenceMode(configuredMode, chunk.mark, emphAdj);

        size_t spokenIdx = 0;
        for (size_t ti = 0; ti < tokens.size(); ++ti) {
            const auto& tok = tokens[ti];
            if (tok.type == TokenType::PauseShort) { AppendSilence(output, static_cast<uint32_t>(kSampleRate * 0.040 * rateMult)); continue; }
            if (tok.type == TokenType::PauseLong) { AppendSilence(output, static_cast<uint32_t>(kSampleRate * 0.080 * rateMult)); continue; }

            double pos = speechCount <= 1 ? 0.0 : static_cast<double>(spokenIdx) / static_cast<double>(speechCount - 1);
            ++spokenIdx;
            double localPitch = std::max(28.0, basePitchHz * ContourFactor(pos, sentenceMode, contourStrength));

            double dur = 0.10 * rateMult;
            bool prevConsonant = false, nextConsonant = false;
            if (ti > 0 && tokens[ti - 1].type == TokenType::Phone && !IsVowelPhone(tokens[ti - 1].p)) prevConsonant = true;
            if (ti + 1 < tokens.size() && tokens[ti + 1].type == TokenType::Phone && !IsVowelPhone(tokens[ti + 1].p)) nextConsonant = true;
            const bool inConsonantCluster = (!IsVowelPhone(tok.p) && (prevConsonant || nextConsonant));
            if (tok.p == L"m" || tok.p == L"n" || tok.p == L"ń" || tok.p == L"l" || tok.p == L"ł" || tok.p == L"r" || tok.p == L"j" || tok.p == L"w") dur = 0.085 * rateMult;
            if (tok.p == L"p" || tok.p == L"b" || tok.p == L"t" || tok.p == L"d" || tok.p == L"k" || tok.p == L"g" || tok.p == L"c" || tok.p == L"ć" || tok.p == L"cz" || tok.p == L"dz" || tok.p == L"dź" || tok.p == L"dż") dur = 0.06 * rateMult;
            const bool emphaticPhone = (tok.p == L"sz" || tok.p == L"cz" || tok.p == L"k" || tok.p == L"t" || tok.p == L"ś" || tok.p == L"dź" || tok.p == L"dż" || tok.p == L"ć" || tok.p == L"ł" || tok.p == L"ę" || tok.p == L"ż");
            if (inConsonantCluster) dur *= 1.28;
            if (emphaticPhone) dur *= 1.12;
            if (IsVowelPhone(tok.p) && nextConsonant) dur *= 1.08;
            const bool nextIsSz = (ti + 1 < tokens.size() && tokens[ti + 1].type == TokenType::Phone && tokens[ti + 1].p == L"sz");
            const bool nextIsK = (ti + 1 < tokens.size() && tokens[ti + 1].type == TokenType::Phone && tokens[ti + 1].p == L"k");
            const bool nextIsT = (ti + 1 < tokens.size() && tokens[ti + 1].type == TokenType::Phone && tokens[ti + 1].p == L"t");
            const bool prevIsR = (ti > 0 && tokens[ti - 1].type == TokenType::Phone && tokens[ti - 1].p == L"r");
            if (tok.p == L"u" && nextIsSz) dur *= 1.45;
            if (tok.p == L"r" && (nextIsK || nextIsT)) dur *= 1.35;
            if ((tok.p == L"k" || tok.p == L"t") && prevIsR) dur *= 1.22;

            if (tok.p == L"r") {
                // Keep Polish /r/ (alveolar tap/trill) crisp, especially before voiceless stops.
                int taps = 3;
                double rF1 = 430.0, rF2 = 1180.0, rF3 = 2350.0;
                double rGap = 0.006;
                float rAmp = 1.0f;
                if (nextIsK || nextIsT) {
                    taps = 4;
                    rF2 = 1360.0; rF3 = 2660.0;
                    rGap = 0.004;
                    rAmp = 1.12f;
                }
                for (int j = 0; j < taps; ++j) {
                    auto pulse = GenRectPulse(rF1, rF2, rF3, static_cast<int>(kSampleRate / (localPitch * 3.3)));
                    for (float v : pulse) output.push_back(v * rAmp);
                    AppendSilence(output, static_cast<uint32_t>(kSampleRate * rGap));
                }
                if (nextIsK || nextIsT) {
                    AppendNoise(output, static_cast<uint32_t>(kSampleRate * 0.003), unvoicedNoiseGain * 0.22f, rng, 0.25f);
                }
                continue;
            }

            auto formants = GetFormants(tok.p);
            if (formants[0] > 0.0) {
                int pulseLen = std::max(1, static_cast<int>(kSampleRate / localPitch));
                int cycles = std::max(1, static_cast<int>((kSampleRate * dur) / pulseLen));
                for (int c = 0; c < cycles; ++c) {
                    for (int i = 0; i < 3; ++i) curF[i] += (formants[i] - curF[i]) * formantSlew;
                    auto pulse = GenRectPulse(curF[0], curF[1], curF[2], pulseLen);
                    output.insert(output.end(), pulse.begin(), pulse.end());
                }
            } else if (tok.p == L"s" || tok.p == L"sz" || tok.p == L"ś" || tok.p == L"f" || tok.p == L"h") {
                float amp = 0.18f;
                float sparse = 0.20f;
                if (tok.p == L"sz") { amp = unvoicedNoiseGain * 1.35f; sparse = 0.36f; }
                else if (tok.p == L"ś") { amp = unvoicedNoiseGain * 1.18f; sparse = 0.32f; }
                else if (tok.p == L"h") { amp = unvoicedNoiseGain * 0.85f; sparse = 0.18f; }
                uint32_t len = static_cast<uint32_t>(kSampleRate * dur * ((tok.p == L"sz" || tok.p == L"ś") ? 1.18 : 1.0));
                const bool prevIsU = (ti > 0 && tokens[ti - 1].type == TokenType::Phone && tokens[ti - 1].p == L"u");
                size_t tj = ti + 1;
                while (tj < tokens.size() && tokens[tj].type == TokenType::PauseShort) ++tj;
                const bool endLike = (tj >= tokens.size() || tokens[tj].type == TokenType::PauseLong);
                if (tok.p == L"sz" && (prevIsU || endLike)) {
                    amp *= 1.12f;
                    len = static_cast<uint32_t>(len * 1.24);
                    sparse = std::max(0.38f, sparse);
                }
                AppendNoise(output, len, amp, rng, sparse);
            } else if (tok.p == L"z" || tok.p == L"ż" || tok.p == L"ź" || tok.p == L"w") {
                auto base = GenRectPulse(380.0, 1400.0, 2300.0, static_cast<int>(kSampleRate / localPitch));
                int cycles = std::max(1, static_cast<int>((kSampleRate * dur) / std::max<size_t>(1, base.size())));
                std::uniform_real_distribution<float> u(-1.0f, 1.0f), c(0.0f, 1.0f);
                float sparse = (tok.p == L"ź") ? 0.38f : 0.28f;
                float voiceMix = 0.72f;
                float noiseMix = voicedNoiseGain;
                if (tok.p == L"ż") { voiceMix = 0.66f; noiseMix = voicedNoiseGain * 1.5f; sparse = 0.36f; }
                if (tok.p == L"ź") { voiceMix = 0.68f; noiseMix = voicedNoiseGain * 1.35f; }
                for (int cc = 0; cc < cycles; ++cc) {
                    for (float v : base) {
                        float n = u(rng); if (c(rng) > sparse) n = 0.0f;
                        output.push_back(v * voiceMix + n * noiseMix);
                    }
                }
            } else if (tok.p == L"p" || tok.p == L"b" || tok.p == L"t" || tok.p == L"d" || tok.p == L"k" || tok.p == L"g") {
                float closure = 0.011f;
                if (prevIsR && (tok.p == L"t" || tok.p == L"k")) closure = 0.006f;
                AppendSilence(output, static_cast<uint32_t>(kSampleRate * closure));
                float amp = 0.24f;
                uint32_t burstLen = static_cast<uint32_t>(kSampleRate * 0.014);
                if (tok.p == L"t") { amp = 0.57f; burstLen = static_cast<uint32_t>(kSampleRate * 0.018); }
                else if (tok.p == L"k") { amp = 0.56f; burstLen = static_cast<uint32_t>(kSampleRate * 0.018); }
                else if (tok.p == L"p") { amp = 0.36f; burstLen = static_cast<uint32_t>(kSampleRate * 0.014); }
                if (prevIsR && tok.p == L"t") { amp *= 1.12f; burstLen = static_cast<uint32_t>(burstLen * 1.06f); }
                if (prevIsR && tok.p == L"k") { amp *= 1.18f; burstLen = static_cast<uint32_t>(burstLen * 1.12f); }
                AppendNoise(output, burstLen, amp, rng);
                if (tok.p == L"t" || tok.p == L"k") {
                    // Extra aspiration burst to make /t/ and /k/ clearer in clusters and final positions.
                    AppendSilence(output, static_cast<uint32_t>(kSampleRate * 0.002));
                    float asp = amp * 0.40f;
                    if (prevIsR) asp *= 1.15f;
                    uint32_t aspLen = static_cast<uint32_t>(kSampleRate * (prevIsR ? 0.007 : 0.005));
                    AppendNoise(output, aspLen, asp, rng, 0.28f);
                }
            } else if (tok.p == L"cz" || tok.p == L"c" || tok.p == L"ć" || tok.p == L"dz" || tok.p == L"dź" || tok.p == L"dż") {
                AppendSilence(output, static_cast<uint32_t>(kSampleRate * 0.010));
                float affAmp = unvoicedNoiseGain;
                if (tok.p == L"ć" || tok.p == L"dź") affAmp *= 1.45f;
                else if (tok.p == L"dż" || tok.p == L"cz") affAmp *= 1.35f;
                AppendNoise(output, static_cast<uint32_t>(kSampleRate * 0.052 * rateMult), affAmp, rng, 0.22f);
            }
            AppendSilence(output, static_cast<uint32_t>(kSampleRate * (inConsonantCluster ? 0.0045 : 0.003)));
        }

        if (chunk.mark == L'?') AppendSilence(output, static_cast<uint32_t>(kSampleRate * 0.060));
        else if (chunk.mark == L'!') AppendSilence(output, static_cast<uint32_t>(kSampleRate * 0.050));
        else AppendSilence(output, static_cast<uint32_t>(kSampleRate * 0.070));
    }

    if (output.empty()) return {};
    AppendSilence(output, static_cast<uint32_t>(kSampleRate * 0.06));

    float peak = 0.0f;
    for (float v : output) peak = std::max(peak, std::abs(v));
    if (peak < 1e-6f) peak = 1.0f;

    std::vector<uint8_t> pcm;
    pcm.reserve(output.size() * sizeof(int16_t));
    for (float v : output) {
        float norm = (v / peak) * static_cast<float>(volumeMult);
        float shaped = quantizeToC64 ? (std::round(norm * 12.0f) / 12.0f) : norm;
        int sample = static_cast<int>(shaped * 32767.0f);
        int16_t s = static_cast<int16_t>(std::clamp(sample, -32768, 32767));
        const uint8_t* samplePtr = reinterpret_cast<const uint8_t*>(&s);
        pcm.push_back(samplePtr[0]); pcm.push_back(samplePtr[1]);
    }
    return pcm;
}

bool IsPcm16Mono(const WAVEFORMATEX* fmt) {
    return fmt && fmt->wFormatTag == WAVE_FORMAT_PCM && fmt->nChannels == 1 && fmt->wBitsPerSample == 16;
}

class BlackBoxEngine final : public ISpTTSEngine, public ISpObjectWithToken {
public:
    BlackBoxEngine() : refCount_(1), token_(nullptr) { g_objects.fetch_add(1, std::memory_order_relaxed); }
    ~BlackBoxEngine() {
        if (token_) { token_->Release(); token_ = nullptr; }
        g_objects.fetch_sub(1, std::memory_order_relaxed);
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == IID_ISpTTSEngine) *ppv = static_cast<ISpTTSEngine*>(this);
        else if (riid == IID_ISpObjectWithToken) *ppv = static_cast<ISpObjectWithToken*>(this);
        else return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(reinterpret_cast<LONG*>(&refCount_))); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG v = static_cast<ULONG>(InterlockedDecrement(reinterpret_cast<LONG*>(&refCount_)));
        if (v == 0) delete this;
        return v;
    }

    HRESULT STDMETHODCALLTYPE SetObjectToken(ISpObjectToken* pToken) override {
        if (token_) { token_->Release(); token_ = nullptr; }
        if (pToken) { pToken->AddRef(); token_ = pToken; }

        intonationMode_ = IntonationMode::Auto;
        intonationStrength_ = 100;
        symbolLevel_ = SymbolLevel::Most;
        numberMode_ = NumberMode::Cardinal;
        voiceFlavor_ = VoiceFlavor::C64;

        if (token_) {
            wchar_t* v = nullptr;
            if (SUCCEEDED(token_->GetStringValue(L"IntonationMode", &v)) && v) { intonationMode_ = ParseIntonationMode(v); CoTaskMemFree(v); }
            if (SUCCEEDED(token_->GetStringValue(L"SymbolLevel", &v)) && v) { symbolLevel_ = ParseSymbolLevel(v); CoTaskMemFree(v); }
            if (SUCCEEDED(token_->GetStringValue(L"NumberMode", &v)) && v) { numberMode_ = ParseNumberMode(v); CoTaskMemFree(v); }
            ULONG d = 0;
            if (SUCCEEDED(token_->GetDWORD(L"IntonationStrength", &d))) intonationStrength_ = ClampInt(static_cast<int>(d), 20, 200);
            if (SUCCEEDED(token_->GetStringValue(L"VoiceFlavor", &v)) && v) { voiceFlavor_ = ParseVoiceFlavor(v); CoTaskMemFree(v); }
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetObjectToken(ISpObjectToken** ppToken) override {
        if (!ppToken) return E_POINTER;
        *ppToken = token_;
        if (token_) token_->AddRef();
        return token_ ? S_OK : S_FALSE;
    }

    HRESULT STDMETHODCALLTYPE Speak(DWORD, REFGUID, const WAVEFORMATEX* pWaveFormatEx, const SPVTEXTFRAG* pTextFragList, ISpTTSEngineSite* pOutputSite) override {
        if (!pOutputSite) return E_POINTER;
        if ((pOutputSite->GetActions() & SPVES_ABORT) != 0) return S_OK;

        std::wstring text = JoinSpeakText(pTextFragList);
        if (text.empty()) return S_OK;

        long rateAdj = 0; (void)pOutputSite->GetRate(&rateAdj);
        USHORT vol = 100; (void)pOutputSite->GetVolume(&vol);

        auto pcm = NativeSynthesize(text, RateAdjustToPercent(rateAdj), PitchAdjustToPercent(pTextFragList), ClampInt(static_cast<int>(vol), 0, 100), intonationMode_, intonationStrength_, symbolLevel_, numberMode_, EmphasisAdjust(pTextFragList), voiceFlavor_);
        if (pcm.empty()) return S_OK;

        (void)pWaveFormatEx;
        ULONG written = 0;
        return pOutputSite->Write(pcm.data(), static_cast<ULONG>(pcm.size()), &written);
    }

    HRESULT STDMETHODCALLTYPE GetOutputFormat(const GUID* pTargetFmtId, const WAVEFORMATEX* pTargetWaveFormatEx, GUID* pOutputFormatId, WAVEFORMATEX** ppCoMemOutputWaveFormatEx) override {
        if (!pOutputFormatId || !ppCoMemOutputWaveFormatEx) return E_POINTER;
        *ppCoMemOutputWaveFormatEx = nullptr;
        auto* fmt = static_cast<WAVEFORMATEX*>(CoTaskMemAlloc(sizeof(WAVEFORMATEX)));
        if (!fmt) return E_OUTOFMEMORY;
        if (pTargetFmtId && *pTargetFmtId == SPDFID_WaveFormatEx && IsPcm16Mono(pTargetWaveFormatEx)) *fmt = *pTargetWaveFormatEx;
        else {
            fmt->wFormatTag = WAVE_FORMAT_PCM; fmt->nChannels = 1; fmt->nSamplesPerSec = kSampleRate; fmt->wBitsPerSample = 16;
            fmt->nBlockAlign = static_cast<WORD>((fmt->nChannels * fmt->wBitsPerSample) / 8); fmt->nAvgBytesPerSec = fmt->nSamplesPerSec * fmt->nBlockAlign; fmt->cbSize = 0;
        }
        *pOutputFormatId = SPDFID_WaveFormatEx;
        *ppCoMemOutputWaveFormatEx = fmt;
        return S_OK;
    }

private:
    ULONG refCount_;
    ISpObjectToken* token_;
    IntonationMode intonationMode_ = IntonationMode::Auto;
    int intonationStrength_ = 100;
    SymbolLevel symbolLevel_ = SymbolLevel::Most;
    NumberMode numberMode_ = NumberMode::Cardinal;
    VoiceFlavor voiceFlavor_ = VoiceFlavor::C64;
};
class ClassFactory final : public IClassFactory {
public:
    ClassFactory() : refCount_(1) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(reinterpret_cast<LONG*>(&refCount_))); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG v = static_cast<ULONG>(InterlockedDecrement(reinterpret_cast<LONG*>(&refCount_)));
        if (v == 0) delete this;
        return v;
    }

    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (outer != nullptr) return CLASS_E_NOAGGREGATION;
        auto* engine = new (std::nothrow) BlackBoxEngine();
        if (!engine) return E_OUTOFMEMORY;
        HRESULT hr = engine->QueryInterface(riid, ppv);
        engine->Release();
        return hr;
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override {
        if (lock) g_locks.fetch_add(1, std::memory_order_relaxed);
        else g_locks.fetch_sub(1, std::memory_order_relaxed);
        return S_OK;
    }

private:
    ULONG refCount_;
};

} // namespace

extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_module = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
    }
    return TRUE;
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (rclsid != CLSID_BlackBoxSapi5) return CLASS_E_CLASSNOTAVAILABLE;
    auto* factory = new (std::nothrow) ClassFactory();
    if (!factory) return E_OUTOFMEMORY;
    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

extern "C" HRESULT __stdcall DllCanUnloadNow(void) {
    return (g_objects.load(std::memory_order_relaxed) == 0 && g_locks.load(std::memory_order_relaxed) == 0) ? S_OK : S_FALSE;
}

extern "C" HRESULT __stdcall DllRegisterServer(void) {
    wchar_t clsid[64] = {0};
    if (StringFromGUID2(CLSID_BlackBoxSapi5, clsid, static_cast<int>(std::size(clsid))) == 0) return E_FAIL;

    wchar_t modulePath[MAX_PATH] = {0};
    if (GetModuleFileNameW(g_module, modulePath, static_cast<DWORD>(std::size(modulePath))) == 0) return HRESULT_FROM_WIN32(GetLastError());

    wchar_t clsidKey[256] = {0};
    swprintf_s(clsidKey, L"CLSID\\%ls", clsid);

    HRESULT hr = WriteRegString(HKEY_CLASSES_ROOT, clsidKey, nullptr, kEngineName);
    if (FAILED(hr)) return hr;

    wchar_t inprocKey[300] = {0};
    swprintf_s(inprocKey, L"%ls\\InprocServer32", clsidKey);

    hr = WriteRegString(HKEY_CLASSES_ROOT, inprocKey, nullptr, modulePath);
    if (FAILED(hr)) return hr;

    return WriteRegString(HKEY_CLASSES_ROOT, inprocKey, L"ThreadingModel", L"Both");
}

extern "C" HRESULT __stdcall DllUnregisterServer(void) {
    wchar_t clsid[64] = {0};
    if (StringFromGUID2(CLSID_BlackBoxSapi5, clsid, static_cast<int>(std::size(clsid))) == 0) return E_FAIL;
    wchar_t clsidKey[256] = {0};
    swprintf_s(clsidKey, L"CLSID\\%ls", clsid);
    return DeleteRegTree(HKEY_CLASSES_ROOT, clsidKey);
}
