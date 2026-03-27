#include <windows.h>
#include <commctrl.h>

#include <array>
#include <string>

namespace {

constexpr wchar_t kWindowClass[] = L"BlackBoxSapi5ConfigWindow";
constexpr wchar_t kWindowTitle[] = L"BlackBox V8 - Ustawienia SAPI5";
constexpr wchar_t kSettingsSubKey[] = L"Software\\BlackBox\\SAPI5\\Settings";

enum ControlId : int {
    IDC_SPEED_TRACK = 1001,
    IDC_SPEED_VALUE,
    IDC_PITCH_TRACK,
    IDC_PITCH_VALUE,
    IDC_MOD_TRACK,
    IDC_MOD_VALUE,
    IDC_VOLUME_TRACK,
    IDC_VOLUME_VALUE,
    IDC_EMOJI_CHECK,
    IDC_SAVE_BUTTON,
    IDC_RESET_BUTTON,
    IDC_CLOSE_BUTTON,
};

struct SliderDef {
    int trackId;
    int valueId;
    const wchar_t* label;
    const wchar_t* settingName;
    int defaultValue;
};

constexpr std::array<SliderDef, 4> kSliders = {{
    {IDC_SPEED_TRACK, IDC_SPEED_VALUE, L"Prędkość", L"SpeedPercent", 50},
    {IDC_PITCH_TRACK, IDC_PITCH_VALUE, L"Wysokość", L"PitchPercent", 50},
    {IDC_MOD_TRACK, IDC_MOD_VALUE, L"Modulacja", L"ModulationPercent", 50},
    {IDC_VOLUME_TRACK, IDC_VOLUME_VALUE, L"Głośność", L"VolumePercent", 100},
}};

HFONT GetUiFont() {
    return static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}

void ApplyUiFont(HWND hwnd) {
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(GetUiFont()), TRUE);
}

int ReadSettingOrDefault(const wchar_t* valueName, int defaultValue) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LONG rc = RegGetValueW(HKEY_CURRENT_USER, kSettingsSubKey, valueName, RRF_RT_REG_DWORD, nullptr, &value, &size);
    if (rc != ERROR_SUCCESS) {
        return defaultValue;
    }
    if (value > 100) {
        value = 100;
    }
    return static_cast<int>(value);
}

bool WriteSetting(const wchar_t* valueName, int value) {
    HKEY key = nullptr;
    const LONG rc = RegCreateKeyExW(HKEY_CURRENT_USER, kSettingsSubKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (rc != ERROR_SUCCESS) {
        return false;
    }
    const DWORD out = static_cast<DWORD>(value);
    const LONG setRc = RegSetValueExW(key, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&out), sizeof(out));
    RegCloseKey(key);
    return setRc == ERROR_SUCCESS;
}

void SetSliderValue(HWND hwnd, const SliderDef& slider, int value) {
    value = (value < 0) ? 0 : ((value > 100) ? 100 : value);
    SendDlgItemMessageW(hwnd, slider.trackId, TBM_SETPOS, TRUE, value);
    wchar_t text[32] = {0};
    wsprintfW(text, L"%d%%", value);
    SetDlgItemTextW(hwnd, slider.valueId, text);
}

int GetSliderValue(HWND hwnd, const SliderDef& slider) {
    return static_cast<int>(SendDlgItemMessageW(hwnd, slider.trackId, TBM_GETPOS, 0, 0));
}

void LoadAllSettings(HWND hwnd) {
    for (const auto& slider : kSliders) {
        SetSliderValue(hwnd, slider, ReadSettingOrDefault(slider.settingName, slider.defaultValue));
    }
    CheckDlgButton(hwnd, IDC_EMOJI_CHECK, ReadSettingOrDefault(L"SpeakEmoji", 1) ? BST_CHECKED : BST_UNCHECKED);
}

void ResetAllSettings(HWND hwnd) {
    for (const auto& slider : kSliders) {
        SetSliderValue(hwnd, slider, slider.defaultValue);
    }
    CheckDlgButton(hwnd, IDC_EMOJI_CHECK, BST_CHECKED);
}

void SaveAllSettings(HWND hwnd) {
    bool ok = true;
    for (const auto& slider : kSliders) {
        ok = WriteSetting(slider.settingName, GetSliderValue(hwnd, slider)) && ok;
    }
    ok = WriteSetting(L"SpeakEmoji", IsDlgButtonChecked(hwnd, IDC_EMOJI_CHECK) == BST_CHECKED ? 1 : 0) && ok;
    MessageBoxW(
        hwnd,
        ok ? L"Zapisano ustawienia. Silnik odczyta je przy następnej wypowiedzi."
           : L"Nie udało się zapisać wszystkich ustawień.",
        L"BlackBox V8",
        ok ? MB_OK | MB_ICONINFORMATION : MB_OK | MB_ICONERROR
    );
}

HWND CreateLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h, int id = 0) {
    HWND hwnd = CreateWindowExW(0, WC_STATICW, text, WS_CHILD | WS_VISIBLE, x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    ApplyUiFont(hwnd);
    return hwnd;
}

HWND CreateButton(HWND parent, int id, const wchar_t* text, int x, int y, int w, int h) {
    const DWORD style = (id == IDC_SAVE_BUTTON) ? (WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON) : (WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON);
    HWND hwnd = CreateWindowExW(0, WC_BUTTONW, text, style, x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    ApplyUiFont(hwnd);
    return hwnd;
}

HWND CreateCheckBox(HWND parent, int id, const wchar_t* text, int x, int y, int w, int h) {
    HWND hwnd = CreateWindowExW(0, WC_BUTTONW, text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    ApplyUiFont(hwnd);
    return hwnd;
}

HWND CreateTrack(HWND parent, int id, int x, int y, int w, int h) {
    HWND hwnd = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_AUTOTICKS, x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    SendMessageW(hwnd, TBM_SETRANGEMIN, TRUE, 0);
    SendMessageW(hwnd, TBM_SETRANGEMAX, TRUE, 100);
    SendMessageW(hwnd, TBM_SETTICFREQ, 10, 0);
    SendMessageW(hwnd, TBM_SETPAGESIZE, 0, 10);
    ApplyUiFont(hwnd);
    return hwnd;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        CreateLabel(hwnd, L"Ustawienia są niezależne od suwaków NVDA i działają jako baza dla głosu.", 16, 14, 500, 20);

        int y = 48;
        for (const auto& slider : kSliders) {
            const HWND label = CreateLabel(hwnd, slider.label, 16, y + 4, 110, 20);
            const HWND track = CreateTrack(hwnd, slider.trackId, 130, y, 250, 36);
            const HWND value = CreateLabel(hwnd, L"0%", 392, y + 4, 56, 20, slider.valueId);
            SendMessageW(track, TBM_SETBUDDY, TRUE, reinterpret_cast<LPARAM>(label));
            SendMessageW(track, TBM_SETBUDDY, FALSE, reinterpret_cast<LPARAM>(value));
            y += 52;
        }

        CreateCheckBox(hwnd, IDC_EMOJI_CHECK, L"Odczytuj emotikony i emoji", 130, y + 4, 250, 24);
        CreateLabel(hwnd, L"Gdy wyłączone, emoji i emotikony będą pomijane jako zwykły tekst.", 130, y + 30, 320, 20);
        y += 56;

        CreateButton(hwnd, IDC_SAVE_BUTTON, L"Zapisz", 130, y + 10, 100, 28);
        CreateButton(hwnd, IDC_RESET_BUTTON, L"Domyślne", 240, y + 10, 100, 28);
        CreateButton(hwnd, IDC_CLOSE_BUTTON, L"Zamknij", 350, y + 10, 100, 28);

        LoadAllSettings(hwnd);
        PostMessageW(hwnd, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(GetDlgItem(hwnd, IDC_SPEED_TRACK)), TRUE);
        return 0;
    }
    case WM_HSCROLL: {
        const HWND source = reinterpret_cast<HWND>(lParam);
        for (const auto& slider : kSliders) {
            if (source == GetDlgItem(hwnd, slider.trackId)) {
                SetSliderValue(hwnd, slider, GetSliderValue(hwnd, slider));
                break;
            }
        }
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_SAVE_BUTTON:
            SaveAllSettings(hwnd);
            return 0;
        case IDC_RESET_BUTTON:
            ResetAllSettings(hwnd);
            return 0;
        case IDC_CLOSE_BUTTON:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClass;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_CONTROLPARENT,
        kWindowClass,
        kWindowTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        490,
        390,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (!hwnd) {
        return 1;
    }

    ShowWindow(hwnd, showCommand == 0 ? SW_SHOWDEFAULT : showCommand);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (IsDialogMessageW(hwnd, &msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
