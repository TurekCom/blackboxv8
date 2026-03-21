#include "blackbox/text.hpp"

#include <array>
#include <cwctype>
#include <string>
#include <vector>

namespace blackbox {
namespace {

wchar_t LowerPLChar(wchar_t ch) {
    switch (ch) {
    case L'Ą': return L'ą';
    case L'Ć': return L'ć';
    case L'Ę': return L'ę';
    case L'Ł': return L'ł';
    case L'Ń': return L'ń';
    case L'Ó': return L'ó';
    case L'Ś': return L'ś';
    case L'Ź': return L'ź';
    case L'Ż': return L'ż';
    default: return static_cast<wchar_t>(std::towlower(ch));
    }
}

bool IsAsciiAlphaNum(wchar_t ch) {
    return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9');
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

std::wstring TripletToWords(int n) {
    static const std::array<const wchar_t*, 10> kHundreds = {
        L"", L"sto", L"dwieście", L"trzysta", L"czterysta",
        L"pięćset", L"sześćset", L"siedemset", L"osiemset", L"dziewięćset",
    };
    static const std::array<const wchar_t*, 10> kTens = {
        L"", L"dziesięć", L"dwadzieścia", L"trzydzieści", L"czterdzieści",
        L"pięćdziesiąt", L"sześćdziesiąt", L"siedemdziesiąt", L"osiemdziesiąt", L"dziewięćdziesiąt",
    };
    static const std::array<const wchar_t*, 10> kUnits = {
        L"zero", L"jeden", L"dwa", L"trzy", L"cztery",
        L"pięć", L"sześć", L"siedem", L"osiem", L"dziewięć",
    };
    static const std::array<const wchar_t*, 10> kTeens = {
        L"dziesięć", L"jedenaście", L"dwanaście", L"trzynaście", L"czternaście",
        L"piętnaście", L"szesnaście", L"siedemnaście", L"osiemnaście", L"dziewiętnaście",
    };

    const int hundreds = n / 100;
    const int tens = (n / 10) % 10;
    const int units = n % 10;

    std::wstring out;
    if (hundreds > 0) {
        out += kHundreds[hundreds];
    }
    if (tens == 1) {
        if (!out.empty()) {
            out += L' ';
        }
        out += kTeens[units];
        return out;
    }
    if (tens > 0) {
        if (!out.empty()) {
            out += L' ';
        }
        out += kTens[tens];
    }
    if (units > 0) {
        if (!out.empty()) {
            out += L' ';
        }
        out += kUnits[units];
    }
    return out;
}

const wchar_t* PluralForm(int n, const wchar_t* one, const wchar_t* few, const wchar_t* many) {
    if (n == 1) {
        return one;
    }
    const int last2 = n % 100;
    const int last = n % 10;
    if (last2 > 10 && last2 < 20) {
        return many;
    }
    if (last >= 2 && last <= 4) {
        return few;
    }
    return many;
}

std::wstring IntToPolishWords(long long n) {
    if (n == 0) {
        return L"zero";
    }
    if (n < 0) {
        return L"minus " + IntToPolishWords(-n);
    }

    std::vector<std::wstring> parts;

    int chunk = static_cast<int>(n % 1000);
    if (chunk > 0) {
        parts.push_back(TripletToWords(chunk));
    }
    n /= 1000;

    int group = 1;
    while (n > 0) {
        chunk = static_cast<int>(n % 1000);
        if (chunk > 0) {
            std::wstring word = TripletToWords(chunk);
            if (group == 1) {
                word = (chunk == 1) ? L"tysiąc" : word + L" " + PluralForm(chunk, L"tysiąc", L"tysiące", L"tysięcy");
            } else if (group == 2) {
                word += L" " + std::wstring(PluralForm(chunk, L"milion", L"miliony", L"milionów"));
            } else if (group == 3) {
                word += L" " + std::wstring(PluralForm(chunk, L"miliard", L"miliardy", L"miliardów"));
            } else if (group == 4) {
                word += L" " + std::wstring(PluralForm(chunk, L"bilion", L"biliony", L"bilionów"));
            }
            parts.push_back(word);
        }
        n /= 1000;
        ++group;
    }

    std::wstring out;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
        if (!out.empty()) {
            out += L' ';
        }
        out += *it;
    }
    return out;
}

std::wstring DigitsToPolishWords(std::wstring_view digits) {
    static const std::array<const wchar_t*, 10> kUnits = {
        L"zero", L"jeden", L"dwa", L"trzy", L"cztery",
        L"pięć", L"sześć", L"siedem", L"osiem", L"dziewięć",
    };

    std::wstring out;
    for (wchar_t ch : digits) {
        if (ch == L'-') {
            if (!out.empty()) {
                out += L' ';
            }
            out += L"minus";
            continue;
        }
        if (ch >= L'0' && ch <= L'9') {
            if (!out.empty()) {
                out += L' ';
            }
            out += kUnits[static_cast<size_t>(ch - L'0')];
        }
    }
    return out;
}

}  // namespace

std::wstring ToLowerPL(std::wstring_view text) {
    std::wstring out;
    out.reserve(text.size());
    for (wchar_t ch : text) {
        out.push_back(LowerPLChar(ch));
    }
    return out;
}

std::wstring ExpandNumbers(std::wstring_view text, NumberMode mode) {
    std::wstring out;
    size_t i = 0;
    while (i < text.size()) {
        const wchar_t ch = text[i];
        const bool minus = (ch == L'-' && i + 1 < text.size() && std::iswdigit(text[i + 1]));
        if (std::iswdigit(ch) || minus) {
            const size_t start = i;
            size_t j = i + (minus ? 1 : 0);
            while (j < text.size() && std::iswdigit(text[j])) {
                ++j;
            }
            const wchar_t left = (start > 0) ? text[start - 1] : L' ';
            const wchar_t right = (j < text.size()) ? text[j] : L' ';
            if (!IsAsciiAlphaNum(left) && !IsAsciiAlphaNum(right)) {
                try {
                    const std::wstring raw{text.substr(start, j - start)};
                    out += L' ';
                    out += (mode == NumberMode::Digits) ? DigitsToPolishWords(raw) : IntToPolishWords(std::stoll(raw));
                    out += L' ';
                    i = j;
                    continue;
                } catch (...) {
                }
            }
        }
        out.push_back(ch);
        ++i;
    }
    return out;
}

std::wstring NormalizePolishText(std::wstring_view text) {
    std::wstring s = ToLowerPL(text);

    ReplaceAll(s, L"ó", L"u");
    ReplaceAll(s, L"ch", L"h");
    ReplaceAll(s, L"rz", L"ż");

    auto soften = [&s](const std::wstring& pat, const std::wstring& repl) {
        static const std::array<wchar_t, 8> kVowels = {L'a', L'e', L'i', L'o', L'u', L'y', L'ą', L'ę'};
        for (wchar_t vowel : kVowels) {
            ReplaceAll(s, pat + std::wstring(1, vowel), repl + std::wstring(1, vowel));
        }
        ReplaceAll(s, pat, repl + L"i");
    };

    soften(L"ci", L"ć");
    soften(L"si", L"ś");
    soften(L"zi", L"ź");
    soften(L"ni", L"ń");
    soften(L"dzi", L"dź");

    std::wstring nasal;
    nasal.reserve(s.size() + 8);
    for (size_t i = 0; i < s.size(); ++i) {
        const wchar_t ch = s[i];
        const wchar_t nxt = (i + 1 < s.size()) ? s[i + 1] : L' ';
        if (ch == L'ę') {
            if (nxt == L' ') {
                nasal += L"e";
            } else if (nxt == L'p' || nxt == L'b') {
                nasal += L"em";
            } else if (nxt == L't' || nxt == L'd' || nxt == L'c' || nxt == L'z') {
                nasal += L"en";
            } else {
                nasal += L"ę";
            }
        } else if (ch == L'ą') {
            if (nxt == L' ') {
                nasal += L"oł";
            } else if (nxt == L'p' || nxt == L'b') {
                nasal += L"om";
            } else if (nxt == L't' || nxt == L'd' || nxt == L'c' || nxt == L'z') {
                nasal += L"on";
            } else {
                nasal += L"ą";
            }
        } else {
            nasal.push_back(ch);
        }
    }

    return nasal;
}

}  // namespace blackbox
