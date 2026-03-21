#include <windows.h>
#include <sapi.h>
#include <sapiddk.h>

#include "blackbox/prosody.hpp"
#include "blackbox/sam_like.hpp"
#include "blackbox/text.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <fstream>
#include <new>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

const CLSID CLSID_BlackBoxSapi5 =
{ 0x9b5d8344, 0x4d5e, 0x46a2, { 0x80, 0xd5, 0x2d, 0x83, 0xcc, 0x6b, 0xc2, 0x7d } };
const GUID kSpdfidWaveFormatEx =
{ 0xC31ADBAE, 0x527F, 0x4FF5, { 0xA2, 0x30, 0xF6, 0x2B, 0xB6, 0x1F, 0xF7, 0x0C } };

constexpr wchar_t kEngineName[] = L"BlackBox V8 SAPI5 Engine";
constexpr uint32_t kSampleRate = 22050;
constexpr wchar_t kUserSettingsSubKey[] = L"Software\\BlackBox\\SAPI5\\Settings";

HMODULE g_module = nullptr;
std::atomic<ULONG> g_objects{0};
std::atomic<ULONG> g_locks{0};

enum class IntonationMode { Auto, Flat, Statement, Question, Exclamation };
enum class TokenType { Phone, PauseShort, PauseLong };
enum class VoiceFlavor { C64, Clear };
enum class SymbolLevel { None, Some, Most, All };

struct Token { TokenType type; std::wstring p; };
struct SentenceChunk { std::wstring text; wchar_t mark; };
struct UserVoiceSettings {
    int speedPercent = 50;
    int pitchPercent = 50;
    int modulationPercent = 50;
    int volumePercent = 100;
};

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
bool IsSpeechAction(SPVACTIONS action) { return action == SPVA_Speak || action == SPVA_Pronounce || action == SPVA_SpellOut; }

void ReplaceAll(std::wstring& s, const std::wstring& from, const std::wstring& to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::wstring::npos) { s.replace(pos, from.size(), to); pos += to.size(); }
}

std::wstring JoinSpeakText(const SPVTEXTFRAG* fragList) {
    std::wstring out;
    for (auto f = fragList; f; f = f->pNext) {
        if (f->pTextStart && f->ulTextLen > 0 && IsSpeechAction(f->State.eAction)) {
            out.append(f->pTextStart, f->ulTextLen);
            out.push_back(L' ');
        } else if (f->State.eAction == SPVA_Silence) {
            out.push_back(L' ');
        }
    }
    return out;
}

int AdjustToPercent(long adjust) { return ClampInt(50 + static_cast<int>(adjust) * 5, 0, 100); }
int PercentToSamSpeed(int percent) {
    const double centered = (static_cast<double>(ClampInt(percent, 0, 100)) - 50.0) / 50.0;
    const double factor = std::pow(3.0, -centered);
    return ClampInt(static_cast<int>(std::lround(72.0 * factor)), 24, 180);
}
int ScaleSamSpeedByRateAdjust(int baseSamSpeed, long rateAdjust) {
    const double clamped = std::clamp(static_cast<double>(rateAdjust), -10.0, 10.0);
    const double factor = std::pow(3.0, -clamped / 10.0);
    return ClampInt(static_cast<int>(std::lround(static_cast<double>(baseSamSpeed) * factor)), 24, 180);
}
int PercentToSamPitch(int percent) {
    const double centered = (static_cast<double>(ClampInt(percent, 0, 100)) - 50.0) / 50.0;
    const double factor = std::pow(2.0, -centered);
    return ClampInt(static_cast<int>(std::lround(64.0 * factor)), 24, 104);
}
int AdjustSamPitchByMiddleAdjust(int baseSamPitch, long middleAdj) {
    return ClampInt(baseSamPitch - static_cast<int>(middleAdj) * 3, 24, 104);
}
int PercentToIntonationStrength(int percent) {
    return ClampInt(20 + static_cast<int>(std::lround(1.8 * ClampInt(percent, 0, 100))), 20, 200);
}
int AdjustIntonationStrengthByRange(int baseStrength, long rangeAdj) {
    return ClampInt(baseStrength + static_cast<int>(rangeAdj) * 8, 20, 200);
}
int CombineVolumePercent(int baseVolumePercent, int sapiVolumePercent) {
    return ClampInt((ClampInt(baseVolumePercent, 0, 100) * ClampInt(sapiVolumePercent, 0, 100)) / 100, 0, 100);
}
bool ReadUserDword(const wchar_t* valueName, DWORD* value) {
    if (!value) {
        return false;
    }
    DWORD type = 0;
    DWORD size = sizeof(DWORD);
    const LONG rc = RegGetValueW(HKEY_CURRENT_USER, kUserSettingsSubKey, valueName, RRF_RT_REG_DWORD, &type, value, &size);
    return rc == ERROR_SUCCESS;
}
UserVoiceSettings LoadUserVoiceSettings() {
    UserVoiceSettings settings;
    DWORD value = 0;
    if (ReadUserDword(L"SpeedPercent", &value)) {
        settings.speedPercent = ClampInt(static_cast<int>(value), 0, 100);
    }
    if (ReadUserDword(L"PitchPercent", &value)) {
        settings.pitchPercent = ClampInt(static_cast<int>(value), 0, 100);
    }
    if (ReadUserDword(L"ModulationPercent", &value)) {
        settings.modulationPercent = ClampInt(static_cast<int>(value), 0, 100);
    }
    if (ReadUserDword(L"VolumePercent", &value)) {
        settings.volumePercent = ClampInt(static_cast<int>(value), 0, 100);
    }
    return settings;
}
long AveragePitchMiddleAdj(const SPVTEXTFRAG* fragList) {
    long sum = 0, count = 0;
    for (auto f = fragList; f; f = f->pNext) {
        if (IsSpeechAction(f->State.eAction) && f->pTextStart && f->ulTextLen > 0) {
            sum += f->State.PitchAdj.MiddleAdj; ++count;
        }
    }
    return count > 0 ? (sum / count) : 0;
}
long AveragePitchRangeAdj(const SPVTEXTFRAG* fragList) {
    long sum = 0, count = 0;
    for (auto f = fragList; f; f = f->pNext) {
        if (IsSpeechAction(f->State.eAction) && f->pTextStart && f->ulTextLen > 0) {
            sum += f->State.PitchAdj.RangeAdj; ++count;
        }
    }
    return count > 0 ? (sum / count) : 0;
}
int EmphasisAdjust(const SPVTEXTFRAG* fragList) {
    long sum = 0, count = 0;
    for (auto f = fragList; f; f = f->pNext) {
        if (IsSpeechAction(f->State.eAction) && f->pTextStart && f->ulTextLen > 0) {
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

std::wstring ExpandNumbers(const std::wstring& in, blackbox::NumberMode numberMode) {
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
                    out += (numberMode == blackbox::NumberMode::Digits) ? DigitsToPolishWords(raw) : IntToPolishWords(std::stoll(raw));
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
    std::wstring s = blackbox::NormalizePolishText(sentence);
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
    std::wstring v = blackbox::ToLowerPL(raw);
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
    std::wstring v = blackbox::ToLowerPL(raw);
    if (v == L"clear" || v == L"clean" || v == L"hifi") return VoiceFlavor::Clear;
    return VoiceFlavor::C64;
}

blackbox::NumberMode ParseNumberMode(const wchar_t* raw) {
    if (!raw) return blackbox::NumberMode::Cardinal;
    std::wstring v = blackbox::ToLowerPL(raw);
    if (v == L"digits" || v == L"digit") return blackbox::NumberMode::Digits;
    return blackbox::NumberMode::Cardinal;
}

SymbolLevel ParseSymbolLevel(const wchar_t* raw) {
    if (!raw) return SymbolLevel::Most;
    std::wstring v = blackbox::ToLowerPL(raw);
    if (v == L"none" || v == L"brak") return SymbolLevel::None;
    if (v == L"some" || v == L"czesc" || v == L"część") return SymbolLevel::Some;
    if (v == L"all" || v == L"wszystkie") return SymbolLevel::All;
    return SymbolLevel::Most;
}

blackbox::SamIntonationMode ToSamIntonationMode(IntonationMode mode) {
    switch (mode) {
    case IntonationMode::Flat:
        return blackbox::SamIntonationMode::Flat;
    case IntonationMode::Statement:
        return blackbox::SamIntonationMode::Statement;
    case IntonationMode::Question:
        return blackbox::SamIntonationMode::Question;
    case IntonationMode::Exclamation:
        return blackbox::SamIntonationMode::Exclamation;
    case IntonationMode::Auto:
    default:
        return blackbox::SamIntonationMode::Auto;
    }
}

bool IsVowelPhone(const std::wstring& p) {
    return p == L"a" || p == L"e" || p == L"i" || p == L"o" || p == L"u" || p == L"y" || p == L"ą" || p == L"ę";
}

std::vector<uint8_t> NativeSynthesize(const std::wstring& text, int samSpeed, int samPitch, int samIntonationStrength, int volumePercent, IntonationMode configuredMode, SymbolLevel symbolLevel, blackbox::NumberMode numberMode, int emphAdj, VoiceFlavor flavor) {
    (void)symbolLevel;

    const std::wstring expanded = blackbox::ExpandNumbers(text, numberMode);
    blackbox::SamVoiceSettings settings;
    settings.voice.speed = ClampInt(samSpeed, 24, 180);
    settings.voice.pitch = ClampInt(samPitch, 24, 104);
    settings.voice.mouth = 128;
    settings.voice.throat = 128;
    settings.volume = ClampInt(volumePercent, 0, 100);
    settings.stress = ClampInt(5 + (emphAdj / 3), 1, 8);
    settings.intonationStrength = ClampInt(samIntonationStrength, 20, 200);
    settings.intonationMode = ToSamIntonationMode(configuredMode);
    if (configuredMode == IntonationMode::Auto && emphAdj > 6) {
        settings.intonationMode = blackbox::SamIntonationMode::Exclamation;
    }
    settings.quantizeToC64 = (flavor == VoiceFlavor::C64);
    return blackbox::SynthesizePolishSamLike(expanded, settings);
}

bool IsPcm16Mono(const WAVEFORMATEX* fmt) {
    return fmt && fmt->wFormatTag == WAVE_FORMAT_PCM && fmt->nChannels == 1 && fmt->wBitsPerSample == 16;
}

void MaybeTraceState(long rateAdj, long middleAdj, long rangeAdj, int emphAdj, int ratePercent, int pitchPercent, int rangePercent, int samSpeed, int samPitch, int samIntonationStrength, int finalVolume, const UserVoiceSettings& userSettings) {
    const wchar_t* enabled = _wgetenv(L"BLACKBOX_SAPI_TRACE");
    if (!enabled || *enabled == L'\0' || *enabled == L'0') {
        return;
    }

    std::wofstream trace(L"C:\\Users\\turek\\Desktop\\blackbox\\test_outputs\\sapi_trace.txt", std::ios::app);
    if (!trace) {
        return;
    }
    trace << L"rateAdj=" << rateAdj
          << L" middleAdj=" << middleAdj
          << L" rangeAdj=" << rangeAdj
          << L" emphAdj=" << emphAdj
          << L" ratePercent=" << ratePercent
          << L" pitchPercent=" << pitchPercent
          << L" rangePercent=" << rangePercent
          << L" userSpeed=" << userSettings.speedPercent
          << L" userPitch=" << userSettings.pitchPercent
          << L" userModulation=" << userSettings.modulationPercent
          << L" userVolume=" << userSettings.volumePercent
          << L" samSpeed=" << samSpeed
          << L" samPitch=" << samPitch
          << L" samIntonationStrength=" << samIntonationStrength
          << L" finalVolume=" << finalVolume
          << L"\n";
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
        if (riid == IID_IUnknown || riid == __uuidof(ISpTTSEngine)) *ppv = static_cast<ISpTTSEngine*>(this);
        else if (riid == __uuidof(ISpObjectWithToken)) *ppv = static_cast<ISpObjectWithToken*>(this);
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
        numberMode_ = blackbox::NumberMode::Cardinal;
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
        const UserVoiceSettings userSettings = LoadUserVoiceSettings();
        const long middleAdj = AveragePitchMiddleAdj(pTextFragList);
        const long rangeAdj = AveragePitchRangeAdj(pTextFragList);
        const int emphAdj = EmphasisAdjust(pTextFragList);
        const int ratePercent = AdjustToPercent(rateAdj);
        const int pitchPercent = AdjustToPercent(middleAdj);
        const int rangePercent = AdjustToPercent(rangeAdj);
        const int samSpeed = ScaleSamSpeedByRateAdjust(PercentToSamSpeed(userSettings.speedPercent), rateAdj);
        const int samPitch = AdjustSamPitchByMiddleAdjust(PercentToSamPitch(userSettings.pitchPercent), middleAdj);
        const int samIntonationStrength = AdjustIntonationStrengthByRange(PercentToIntonationStrength(userSettings.modulationPercent), rangeAdj);
        const int finalVolume = CombineVolumePercent(userSettings.volumePercent, static_cast<int>(vol));
        MaybeTraceState(rateAdj, middleAdj, rangeAdj, emphAdj, ratePercent, pitchPercent, rangePercent, samSpeed, samPitch, samIntonationStrength, finalVolume, userSettings);

        auto pcm = NativeSynthesize(
            text,
            samSpeed,
            samPitch,
            samIntonationStrength,
            finalVolume,
            intonationMode_,
            symbolLevel_,
            numberMode_,
            emphAdj,
            voiceFlavor_
        );
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
        if (pTargetFmtId && *pTargetFmtId == kSpdfidWaveFormatEx && IsPcm16Mono(pTargetWaveFormatEx)) *fmt = *pTargetWaveFormatEx;
        else {
            fmt->wFormatTag = WAVE_FORMAT_PCM; fmt->nChannels = 1; fmt->nSamplesPerSec = kSampleRate; fmt->wBitsPerSample = 16;
            fmt->nBlockAlign = static_cast<WORD>((fmt->nChannels * fmt->wBitsPerSample) / 8); fmt->nAvgBytesPerSec = fmt->nSamplesPerSec * fmt->nBlockAlign; fmt->cbSize = 0;
        }
        *pOutputFormatId = kSpdfidWaveFormatEx;
        *ppCoMemOutputWaveFormatEx = fmt;
        return S_OK;
    }

private:
    ULONG refCount_;
    ISpObjectToken* token_;
    IntonationMode intonationMode_ = IntonationMode::Auto;
    int intonationStrength_ = 100;
    SymbolLevel symbolLevel_ = SymbolLevel::Most;
    blackbox::NumberMode numberMode_ = blackbox::NumberMode::Cardinal;
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
