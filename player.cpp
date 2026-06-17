#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <uxtheme.h>
#include <string>
#include <vector>
#include <sstream>
#include <cctype>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>
#include <map>
#include "track_data.h"

#define IDC_PLAY_BTN 1001
#define IDC_PAUSE_BTN 1002
#define IDC_STOP_BTN 1003
#define IDC_STATUS 1004
#define IDC_ROW_INFO 1005
#define IDC_BPM_EDIT 1006
#define IDC_CALIB_EDIT 1007
#define IDC_RELOAD_BTN 1009
#define IDC_IMPORT_BTN 1010
#define IDC_TRACK_EDIT 1011
#define IDC_WINDOW_TREE 1013
#define IDC_REFRESH_TREE 1014
#define IDC_SELECT_WINDOW 1015
#define IDC_LABEL_STATUS 1016
#define IDC_LABEL_ROW 1017
#define IDC_LABEL_DELAY 1018
#define IDC_LABEL_BPM 1019
#define IDC_LABEL_CALIB 1020
#define IDC_LABEL_WINDOW 1021
#define IDC_LABEL_TRACK 1022
#define IDC_STATUSBAR 1023
#define IDC_PROGRESS_BAR 1024
#define IDC_PROGRESS_LABEL 1025

#define COLOR_PRIMARY RGB(52, 152, 219)
#define COLOR_PRIMARY_DARK RGB(41, 128, 185)
#define COLOR_ACCENT RGB(46, 204, 113)
#define COLOR_APP_BG RGB(245, 245, 245)
#define COLOR_PANEL RGB(255, 255, 255)
#define COLOR_TEXT RGB(50, 50, 50)
#define COLOR_TEXT_LIGHT RGB(100, 100, 100)
#define COLOR_BORDER RGB(200, 200, 200)

struct HWNDInfo {
    std::string windowName;
    std::string className;
    HWND handle;
};

class Player {
public:
    HWNDInfo hwnd;
    std::string trackKeys;
    int rows;
    std::vector<std::string> playList;
    int trackDelay;
    int calibration;
    int bpm;
    bool spacePause;
    std::atomic<bool> isPlaying;
    std::atomic<bool> isPaused;
    std::atomic<int> currentRow;
    std::atomic<bool> shouldStop;

    Player() : rows(0), trackDelay(0), calibration(0), bpm(144), spacePause(true),
               isPlaying(false), isPaused(false), currentRow(0), shouldStop(false) {
        hwnd.handle = nullptr;
    }

    bool SetFocusToWindow(HWND hWnd) {
        DWORD currentThreadId = GetCurrentThreadId();
        DWORD targetThreadId = GetWindowThreadProcessId(hWnd, nullptr);
        AttachThreadInput(targetThreadId, currentThreadId, TRUE);
        bool result = SetFocus(hWnd) != nullptr;
        AttachThreadInput(targetThreadId, currentThreadId, FALSE);
        return result;
    }

    WPARAM GetVirtualKeyCode(char key) {
        key = toupper(key);
        switch (key) {
            case 'A': return 0x41; case 'B': return 0x42; case 'C': return 0x43;
            case 'D': return 0x44; case 'E': return 0x45; case 'F': return 0x46;
            case 'G': return 0x47; case 'H': return 0x48; case 'I': return 0x49;
            case 'J': return 0x4A; case 'K': return 0x4B; case 'L': return 0x4C;
            case 'M': return 0x4D; case 'N': return 0x4E; case 'O': return 0x4F;
            case 'P': return 0x50; case 'Q': return 0x51; case 'R': return 0x52;
            case 'S': return 0x53; case 'T': return 0x54; case 'U': return 0x55;
            case 'V': return 0x56; case 'W': return 0x57; case 'X': return 0x58;
            case 'Y': return 0x59; case 'Z': return 0x5A;
            case '0': return 0x30; case '1': return 0x31; case '2': return 0x32;
            case '3': return 0x33; case '4': return 0x34; case '5': return 0x35;
            case '6': return 0x36; case '7': return 0x37; case '8': return 0x38;
            case '9': return 0x39;
            case '-': return 0xBD; case '=': return 0xBB; case '[': return 0xDB;
            case ']': return 0xDD; case ';': return 0xBA; case '\'': return 0xDE;
            case '`': return 0xC0; case ',': return 0xBC; case '.': return 0xBE;
            case '/': return 0xBF; case '\\': return 0xDC;
            default: return 0;
        }
    }

    void SimulateKeyPress(HWND hWnd, const std::string& keys) {
        SetFocusToWindow(hWnd);
        if (keys.length() > 1) {
            for (char key : keys) {
                WPARAM vkCode = GetVirtualKeyCode(key);
                if (vkCode != 0) PostMessageW(hWnd, WM_KEYDOWN, vkCode, 0);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            for (char key : keys) {
                WPARAM vkCode = GetVirtualKeyCode(key);
                if (vkCode != 0) PostMessageW(hWnd, WM_KEYUP, vkCode, 0);
            }
        } else {
            WPARAM vkCode = GetVirtualKeyCode(keys[0]);
            if (vkCode != 0) {
                PostMessageW(hWnd, WM_KEYDOWN, vkCode, 0);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                PostMessageW(hWnd, WM_KEYUP, vkCode, 0);
            }
        }
    }

    void SplitTrackIntoPlayList() {
        playList.clear();
        std::stringstream ss(trackKeys);
        std::string line;
        while (std::getline(ss, line)) {
            playList.push_back(line);
        }
        rows = (int)playList.size();
    }

    void Play() {
        if (rows <= 0 || hwnd.handle == nullptr) return;

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);

        while (!shouldStop) {
            for (int n = 0; n < rows; n++) {
                currentRow = n + 1;
                if (shouldStop) return;
                
                while (isPaused && !shouldStop) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                if (shouldStop) return;

                bool isChord = false;
                int chordStart = 0;
                int delayInterval = trackDelay;
                bool halfNote = false;
                bool quarterNote = false;
                
                const std::string& line = playList[n];
                for (size_t n1 = 0; n1 < line.length(); n1++) {
                    if (shouldStop) return;
                    while (isPaused && !shouldStop) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                    if (shouldStop) return;

                    char c = line[n1];
                    if (isalpha(c)) {
                        if (!isChord) {
                            std::string key(1, toupper(c));
                            SimulateKeyPress(hwnd.handle, key);
                            std::this_thread::sleep_for(std::chrono::milliseconds(delayInterval));
                        }
                    } else {
                        switch (c) {
                            case '(':
                                chordStart = (int)n1;
                                isChord = true;
                                break;
                            case ')': {
                                std::string chord = line.substr(chordStart + 1, n1 - chordStart - 1);
                                for (char& ch : chord) ch = toupper(ch);
                                SimulateKeyPress(hwnd.handle, chord);
                                isChord = false;
                                std::this_thread::sleep_for(std::chrono::milliseconds(delayInterval));
                                break;
                            }
                            case ' ':
                                if (spacePause) {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(delayInterval));
                                }
                                break;
                            case '/':
                                if (halfNote) {
                                    delayInterval = trackDelay;
                                    halfNote = false;
                                } else {
                                    delayInterval = trackDelay / 2;
                                    std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
                                    halfNote = true;
                                }
                                break;
                            case '\\':
                                if (quarterNote) {
                                    delayInterval = trackDelay;
                                    quarterNote = false;
                                } else {
                                    delayInterval = trackDelay / 4;
                                    std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
                                    quarterNote = true;
                                }
                                break;
                            case '-':
                                std::this_thread::sleep_for(std::chrono::milliseconds(trackDelay));
                                break;
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        isPlaying = false;
    }

    void ProcessTrack() {
        size_t pos = trackKeys.find('[');
        while (pos != std::string::npos) {
            trackKeys.replace(pos, 1, "]");
            pos = trackKeys.find('[', pos + 1);
        }
    }

    void Initialize() {
        trackDelay = (int)(60000.0 / bpm) + calibration;
        ProcessTrack();
        SplitTrackIntoPlayList();
    }

    void Start() {
        if (isPlaying && !isPaused) return;
        shouldStop = false;
        isPaused = false;
        if (!isPlaying) {
            isPlaying = true;
            std::thread t(&Player::Play, this);
            t.detach();
        }
    }

    void Pause() {
        isPaused = !isPaused;
    }

    void Stop() {
        shouldStop = true;
        isPlaying = false;
        isPaused = false;
        currentRow = 0;
    }

    bool LoadTrackFromFile(const std::string& filePath) {
        HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;
        
        DWORD fileSize = GetFileSize(hFile, NULL);
        if (fileSize == INVALID_FILE_SIZE) {
            CloseHandle(hFile);
            return false;
        }
        
        std::string content(fileSize, '\0');
        DWORD bytesRead = 0;
        ReadFile(hFile, &content[0], fileSize, &bytesRead, NULL);
        CloseHandle(hFile);
        
        trackKeys = content;
        Initialize();
        return true;
    }
};

Player g_player;
HWND g_hWnd;
std::map<HTREEITEM, HWND> g_treeItemMap;

HFONT g_fontTitle = nullptr;
HFONT g_fontLabel = nullptr;
HFONT g_fontButton = nullptr;

struct ControlHandles {
    HWND titleLabel;
    HWND statusPanel;
    HWND statusLabel;
    HWND statusStatic;
    HWND rowLabel;
    HWND rowStatic;
    HWND delayStatic;
    HWND windowPanel;
    HWND windowLabel;
    HWND windowTree;
    HWND refreshBtn;
    HWND selectBtn;
    HWND paramPanel;
    HWND bpmLabel;
    HWND bpmEdit;
    HWND calibLabel;
    HWND calibEdit;
    HWND importBtn;
    HWND trackPanel;
    HWND trackLabel;
    HWND trackEdit;
    HWND progressPanel;
    HWND progressBar;
    HWND progressLabel;
    HWND btnPanel;
    HWND reloadBtn;
    HWND playBtn;
    HWND pauseBtn;
    HWND stopBtn;
} g_ctrls;

void CreateFonts() {
    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    
    lf.lfHeight = -22;
    lf.lfWeight = FW_BOLD;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei");
    g_fontTitle = CreateFontIndirectW(&lf);
    
    lf.lfHeight = -14;
    lf.lfWeight = FW_NORMAL;
    g_fontLabel = CreateFontIndirectW(&lf);
    
    lf.lfHeight = -14;
    lf.lfWeight = FW_SEMIBOLD;
    g_fontButton = CreateFontIndirectW(&lf);
}

void DeleteFonts() {
    if (g_fontTitle) DeleteObject(g_fontTitle);
    if (g_fontLabel) DeleteObject(g_fontLabel);
    if (g_fontButton) DeleteObject(g_fontButton);
}



LRESULT CALLBACK ModernButtonProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            
            RECT rect;
            GetClientRect(hWnd, &rect);
            
            bool isPressed = (GetWindowLongPtr(hWnd, GWLP_USERDATA) & 0x01) != 0;
            bool isHover = (GetWindowLongPtr(hWnd, GWLP_USERDATA) & 0x02) != 0;
            bool isEnabled = IsWindowEnabled(hWnd);
            
            COLORREF fillColor;
            if (!isEnabled) fillColor = RGB(200, 200, 200);
            else if (isPressed) fillColor = COLOR_PRIMARY_DARK;
            else if (isHover) fillColor = RGB(62, 162, 229);
            else fillColor = COLOR_PRIMARY;
            
            HBRUSH brush = CreateSolidBrush(fillColor);
            SelectObject(hdc, brush);
            RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 6, 6);
            DeleteObject(brush);
            
            HPEN pen = CreatePen(PS_SOLID, 1, COLOR_PRIMARY_DARK);
            SelectObject(hdc, pen);
            RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 6, 6);
            DeleteObject(pen);
            
            WCHAR text[64];
            GetWindowTextW(hWnd, text, 64);
            
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, isEnabled ? RGB(255, 255, 255) : RGB(150, 150, 150));
            SelectObject(hdc, g_fontButton);
            
            SIZE textSize;
            GetTextExtentPoint32W(hdc, text, lstrlenW(text), &textSize);
            int x = (rect.right - rect.left - textSize.cx) / 2;
            int y = (rect.bottom - rect.top - textSize.cy) / 2;
            
            TextOutW(hdc, x, y, text, lstrlenW(text));
            
            EndPaint(hWnd, &ps);
            return 0;
        }
        
        case WM_MOUSEMOVE: {
            SetWindowLongPtr(hWnd, GWLP_USERDATA, GetWindowLongPtr(hWnd, GWLP_USERDATA) | 0x02);
            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            TrackMouseEvent(&tme);
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }
        
        case WM_MOUSELEAVE: {
            SetWindowLongPtr(hWnd, GWLP_USERDATA, GetWindowLongPtr(hWnd, GWLP_USERDATA) & ~0x02);
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }
        
        case WM_LBUTTONDOWN: {
            SetWindowLongPtr(hWnd, GWLP_USERDATA, GetWindowLongPtr(hWnd, GWLP_USERDATA) | 0x01);
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }
        
        case WM_LBUTTONUP: {
            SetWindowLongPtr(hWnd, GWLP_USERDATA, GetWindowLongPtr(hWnd, GWLP_USERDATA) & ~0x01);
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }
        
        case WM_ENABLE:
            InvalidateRect(hWnd, NULL, TRUE);
            break;
            
        case WM_ERASEBKGND:
            return 1;
    }
    
    return DefSubclassProc(hWnd, message, wParam, lParam);
}

HWND CreateModernButton(HINSTANCE hInst, HWND parent, int x, int y, int width, int height, int id, const wchar_t* text) {
    HWND btn = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x, y, width, height, parent, (HMENU)id, hInst, NULL);
    SetWindowSubclass(btn, ModernButtonProc, 0, 0);
    return btn;
}

BOOL CALLBACK EnumWindowsCallback(HWND hWnd, LPARAM lParam) {
    if (!IsWindowVisible(hWnd)) return TRUE;
    
    char title[256];
    char className[256];
    GetWindowTextA(hWnd, title, 256);
    GetClassNameA(hWnd, className, 256);
    
    if (strlen(title) == 0 && strlen(className) == 0) return TRUE;
    
    HWND hTree = (HWND)lParam;
    HTREEITEM hParent = TVI_ROOT;
    
    HWND hOwner = GetParent(hWnd);
    if (hOwner) {
        for (auto& pair : g_treeItemMap) {
            if (pair.second == hOwner) {
                hParent = pair.first;
                break;
            }
        }
    }
    
    std::string itemText = title;
    if (itemText.empty()) itemText = className;
    if (itemText.length() > 80) itemText = itemText.substr(0, 80) + "...";
    
    TVINSERTSTRUCTW tvis;
    ZeroMemory(&tvis, sizeof(TVINSERTSTRUCTW));
    tvis.hParent = hParent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvis.item.pszText = (LPWSTR)itemText.c_str();
    tvis.item.lParam = (LPARAM)hWnd;
    
    HTREEITEM hItem = TreeView_InsertItem(hTree, &tvis);
    if (hItem) {
        g_treeItemMap[hItem] = hWnd;
    }
    
    EnumChildWindows(hWnd, EnumWindowsCallback, lParam);
    return TRUE;
}

void RefreshWindowTree(HWND hTree) {
    TreeView_DeleteAllItems(hTree);
    g_treeItemMap.clear();
    EnumWindows(EnumWindowsCallback, (LPARAM)hTree);
}

void UpdateStatus(HWND hWnd) {
    std::string status;
    if (g_player.isPlaying) {
        status = g_player.isPaused ? "已暂停" : "播放中";
    } else {
        status = "已停止";
    }
    SetWindowTextA(GetDlgItem(hWnd, IDC_STATUS), status.c_str());

    std::string rowInfo = "进度: " + std::to_string(g_player.currentRow) + " / " + std::to_string(g_player.rows);
    SetWindowTextA(GetDlgItem(hWnd, IDC_ROW_INFO), rowInfo.c_str());

    std::string delayInfo = "延迟: " + std::to_string(g_player.trackDelay) + " ms";
    HWND delayStatic = GetDlgItem(hWnd, IDC_LABEL_DELAY);
    if (delayStatic) SetWindowTextA(delayStatic, delayInfo.c_str());

    HWND progressBar = GetDlgItem(hWnd, IDC_PROGRESS_BAR);
    HWND progressLabel = GetDlgItem(hWnd, IDC_PROGRESS_LABEL);
    if (progressBar && progressLabel && g_player.rows > 0) {
        int current = g_player.currentRow.load();
        int total = g_player.rows;
        SendMessage(progressBar, TBM_SETRANGE, TRUE, MAKELPARAM(0, total));
        SendMessage(progressBar, TBM_SETPOS, TRUE, current);
        
        wchar_t label[32];
        swprintf(label, L"%d/%d", current, total);
        SetWindowTextW(progressLabel, label);
    }
}

void SetControlState(HWND hWnd) {
    bool playing = g_player.isPlaying;
    bool paused = g_player.isPaused;
    
    EnableWindow(GetDlgItem(hWnd, IDC_PLAY_BTN), !playing || paused);
    EnableWindow(GetDlgItem(hWnd, IDC_PAUSE_BTN), playing && !paused);
    EnableWindow(GetDlgItem(hWnd, IDC_STOP_BTN), playing);
}

void UpdateTrackEditor(HWND hWnd) {
    SetWindowTextA(GetDlgItem(hWnd, IDC_TRACK_EDIT), g_player.trackKeys.c_str());
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
            CreateFonts();

            SetWindowTheme(hWnd, L"Explorer", NULL);

            g_ctrls.titleLabel = CreateWindowW(L"STATIC", L"🎵 音乐演奏器", WS_CHILD | WS_VISIBLE | SS_CENTER,
                0, 0, 700, 60, hWnd, NULL, hInst, NULL);
            SendMessage(g_ctrls.titleLabel, WM_SETFONT, (WPARAM)g_fontTitle, TRUE);

            g_ctrls.statusPanel = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                20, 80, 660, 80, hWnd, NULL, hInst, NULL);
            SetWindowLongPtr(g_ctrls.statusPanel, GWLP_USERDATA, 1);

            g_ctrls.statusLabel = CreateWindowW(L"STATIC", L"状态:", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                35, 95, 50, 20, hWnd, NULL, hInst, NULL);
            SendMessage(g_ctrls.statusLabel, WM_SETFONT, (WPARAM)g_fontLabel, TRUE);
            
            g_ctrls.statusStatic = CreateWindowW(L"STATIC", L"已停止", WS_CHILD | WS_VISIBLE | SS_LEFT,
                90, 95, 80, 20, hWnd, (HMENU)IDC_STATUS, hInst, NULL);
            SendMessage(g_ctrls.statusStatic, WM_SETFONT, (WPARAM)g_fontLabel, TRUE);

            g_ctrls.rowLabel = CreateWindowW(L"STATIC", L"进度:", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                190, 95, 50, 20, hWnd, NULL, hInst, NULL);
            SendMessage(g_ctrls.rowLabel, WM_SETFONT, (WPARAM)g_fontLabel, TRUE);
            
            g_ctrls.rowStatic = CreateWindowW(L"STATIC", L"0 / 0", WS_CHILD | WS_VISIBLE | SS_LEFT,
                245, 95, 120, 20, hWnd, (HMENU)IDC_ROW_INFO, hInst, NULL);
            SendMessage(g_ctrls.rowStatic, WM_SETFONT, (WPARAM)g_fontLabel, TRUE);

            g_ctrls.delayStatic = CreateWindowW(L"STATIC", L"延迟: 0 ms", WS_CHILD | WS_VISIBLE,
                380, 95, 100, 20, hWnd, (HMENU)IDC_LABEL_DELAY, hInst, NULL);
            SendMessage(g_ctrls.delayStatic, WM_SETFONT, (WPARAM)g_fontLabel, TRUE);

            g_ctrls.windowPanel = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                20, 175, 660, 155, hWnd, NULL, hInst, NULL);
            SetWindowLongPtr(g_ctrls.windowPanel, GWLP_USERDATA, 1);

            g_ctrls.windowLabel = CreateWindowW(L"STATIC", L"目标窗口:", WS_CHILD | WS_VISIBLE,
                35, 190, 70, 20, hWnd, (HMENU)IDC_LABEL_WINDOW, hInst, NULL);
            SendMessage(g_ctrls.windowLabel, WM_SETFONT, (WPARAM)g_fontLabel, TRUE);

            g_ctrls.windowTree = CreateWindowW(WC_TREEVIEWW, L"", WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | WS_VSCROLL,
                35, 215, 420, 110, hWnd, (HMENU)IDC_WINDOW_TREE, hInst, NULL);
            TreeView_SetExtendedStyle(g_ctrls.windowTree, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);
            SetWindowTheme(g_ctrls.windowTree, L"Explorer", NULL);

            g_ctrls.refreshBtn = CreateModernButton(hInst, hWnd, 470, 215, 75, 30, IDC_REFRESH_TREE, L"刷新");
            g_ctrls.selectBtn = CreateModernButton(hInst, hWnd, 560, 215, 75, 30, IDC_SELECT_WINDOW, L"选中");

            g_ctrls.paramPanel = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                20, 345, 660, 60, hWnd, NULL, hInst, NULL);
            SetWindowLongPtr(g_ctrls.paramPanel, GWLP_USERDATA, 1);

            g_ctrls.bpmLabel = CreateWindowW(L"STATIC", L"BPM:", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                35, 360, 40, 20, hWnd, (HMENU)IDC_LABEL_BPM, hInst, NULL);
            SendMessage(g_ctrls.bpmLabel, WM_SETFONT, (WPARAM)g_fontLabel, TRUE);
            
            g_ctrls.bpmEdit = CreateWindowW(L"EDIT", L"144", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER,
                80, 358, 60, 24, hWnd, (HMENU)IDC_BPM_EDIT, hInst, NULL);
            SendMessage(g_ctrls.bpmEdit, EM_LIMITTEXT, 4, 0);

            g_ctrls.calibLabel = CreateWindowW(L"STATIC", L"校准(ms):", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                160, 360, 70, 20, hWnd, (HMENU)IDC_LABEL_CALIB, hInst, NULL);
            SendMessage(g_ctrls.calibLabel, WM_SETFONT, (WPARAM)g_fontLabel, TRUE);
            
            g_ctrls.calibEdit = CreateWindowW(L"EDIT", L"180", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER,
                235, 358, 60, 24, hWnd, (HMENU)IDC_CALIB_EDIT, hInst, NULL);
            SendMessage(g_ctrls.calibEdit, EM_LIMITTEXT, 4, 0);

            g_ctrls.importBtn = CreateModernButton(hInst, hWnd, 315, 358, 90, 24, IDC_IMPORT_BTN, L"导入谱子");

            g_ctrls.trackPanel = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                20, 420, 660, 140, hWnd, NULL, hInst, NULL);
            SetWindowLongPtr(g_ctrls.trackPanel, GWLP_USERDATA, 1);

            g_ctrls.trackLabel = CreateWindowW(L"STATIC", L"谱子编辑:", WS_CHILD | WS_VISIBLE,
                35, 435, 70, 20, hWnd, (HMENU)IDC_LABEL_TRACK, hInst, NULL);
            SendMessage(g_ctrls.trackLabel, WM_SETFONT, (WPARAM)g_fontLabel, TRUE);
            
            g_ctrls.trackEdit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN,
                35, 460, 590, 90, hWnd, (HMENU)IDC_TRACK_EDIT, hInst, NULL);

            g_ctrls.progressPanel = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                20, 560, 660, 40, hWnd, NULL, hInst, NULL);
            SetWindowLongPtr(g_ctrls.progressPanel, GWLP_USERDATA, 1);

            g_ctrls.progressLabel = CreateWindowW(L"STATIC", L"0/0", WS_CHILD | WS_VISIBLE,
                35, 565, 80, 20, hWnd, (HMENU)IDC_PROGRESS_LABEL, hInst, NULL);
            SendMessage(g_ctrls.progressLabel, WM_SETFONT, (WPARAM)g_fontLabel, TRUE);

            g_ctrls.progressBar = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
                130, 565, 520, 25, hWnd, (HMENU)IDC_PROGRESS_BAR, hInst, NULL);
            SendMessage(g_ctrls.progressBar, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
            SendMessage(g_ctrls.progressBar, TBM_SETPOS, TRUE, 0);
            SendMessage(g_ctrls.progressBar, TBM_SETTICFREQ, 10, 0);
            SetWindowTheme(g_ctrls.progressBar, L"", NULL);

            g_ctrls.btnPanel = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                20, 605, 660, 50, hWnd, NULL, hInst, NULL);

            g_ctrls.reloadBtn = CreateModernButton(hInst, hWnd, 20, 610, 100, 35, IDC_RELOAD_BTN, L"重新加载");
            g_ctrls.playBtn = CreateModernButton(hInst, hWnd, 140, 580, 100, 35, IDC_PLAY_BTN, L"播放");
            g_ctrls.pauseBtn = CreateModernButton(hInst, hWnd, 260, 580, 100, 35, IDC_PAUSE_BTN, L"暂停");
            g_ctrls.stopBtn = CreateModernButton(hInst, hWnd, 380, 580, 100, 35, IDC_STOP_BTN, L"停止");

            g_player.trackKeys = trackData;
            g_player.Initialize();
            UpdateStatus(hWnd);
            SetControlState(hWnd);
            UpdateTrackEditor(hWnd);
            RefreshWindowTree(g_ctrls.windowTree);

            SetTimer(hWnd, 1, 500, NULL);
            break;
        }

        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rect;
            GetClientRect(hWnd, &rect);
            FillRect(hdc, &rect, (HBRUSH)CreateSolidBrush(COLOR_APP_BG));
            return 1;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            
            if (width < 400) width = 400;
            if (height < 560) height = 560;
            
            int padding = 20;
            int panelWidth = width - padding * 2;
            
            int titleHeight = 60;
            int statusPanelHeight = 80;
            int windowPanelHeight = 155;
            int paramPanelHeight = 60;
            int progressPanelHeight = 40;
            int btnPanelHeight = 60;
            
            int trackPanelHeight = height - titleHeight - statusPanelHeight - windowPanelHeight - paramPanelHeight - progressPanelHeight - btnPanelHeight - padding * 6;
            if (trackPanelHeight < 60) trackPanelHeight = 60;
            
            SetWindowPos(g_ctrls.titleLabel, NULL, 0, 0, width, titleHeight, SWP_NOZORDER);
            
            int y = titleHeight + padding;
            SetWindowPos(g_ctrls.statusPanel, NULL, padding, y, panelWidth, statusPanelHeight, SWP_NOZORDER);
            
            int statusY = y + 15;
            SetWindowPos(g_ctrls.statusLabel, NULL, padding + 15, statusY, 50, 20, SWP_NOZORDER);
            SetWindowPos(g_ctrls.statusStatic, NULL, padding + 70, statusY, 80, 20, SWP_NOZORDER);
            SetWindowPos(g_ctrls.rowLabel, NULL, padding + 160, statusY, 50, 20, SWP_NOZORDER);
            SetWindowPos(g_ctrls.rowStatic, NULL, padding + 215, statusY, 120, 20, SWP_NOZORDER);
            SetWindowPos(g_ctrls.delayStatic, NULL, padding + 345, statusY, 120, 20, SWP_NOZORDER);
            
            y += statusPanelHeight + padding;
            SetWindowPos(g_ctrls.windowPanel, NULL, padding, y, panelWidth, windowPanelHeight, SWP_NOZORDER);
            
            SetWindowPos(g_ctrls.windowLabel, NULL, padding + 15, y + 15, 70, 20, SWP_NOZORDER);
            
            int treeWidth = panelWidth - 200;
            SetWindowPos(g_ctrls.windowTree, NULL, padding + 15, y + 40, treeWidth, 110, SWP_NOZORDER);
            
            int btnX = padding + treeWidth + 30;
            SetWindowPos(g_ctrls.refreshBtn, NULL, btnX, y + 40, 75, 30, SWP_NOZORDER);
            SetWindowPos(g_ctrls.selectBtn, NULL, btnX + 90, y + 40, 75, 30, SWP_NOZORDER);
            
            y += windowPanelHeight + padding;
            SetWindowPos(g_ctrls.paramPanel, NULL, padding, y, panelWidth, paramPanelHeight, SWP_NOZORDER);
            
            SetWindowPos(g_ctrls.bpmLabel, NULL, padding + 15, y + 18, 40, 20, SWP_NOZORDER);
            SetWindowPos(g_ctrls.bpmEdit, NULL, padding + 60, y + 16, 60, 24, SWP_NOZORDER);
            SetWindowPos(g_ctrls.calibLabel, NULL, padding + 130, y + 18, 70, 20, SWP_NOZORDER);
            SetWindowPos(g_ctrls.calibEdit, NULL, padding + 205, y + 16, 60, 24, SWP_NOZORDER);
            SetWindowPos(g_ctrls.importBtn, NULL, padding + 285, y + 16, 90, 24, SWP_NOZORDER);
            
            y += paramPanelHeight + padding;
            SetWindowPos(g_ctrls.trackPanel, NULL, padding, y, panelWidth, trackPanelHeight, SWP_NOZORDER);
            
            SetWindowPos(g_ctrls.trackLabel, NULL, padding + 15, y + 15, 70, 20, SWP_NOZORDER);
            SetWindowPos(g_ctrls.trackEdit, NULL, padding + 15, y + 40, panelWidth - 30, trackPanelHeight - 55, SWP_NOZORDER);
            
            y += trackPanelHeight + padding;
            SetWindowPos(g_ctrls.progressPanel, NULL, padding, y, panelWidth, progressPanelHeight, SWP_NOZORDER);
            
            SetWindowPos(g_ctrls.progressLabel, NULL, padding + 15, y + 10, 80, 20, SWP_NOZORDER);
            SetWindowPos(g_ctrls.progressBar, NULL, padding + 100, y + 10, panelWidth - 115, 20, SWP_NOZORDER);
            
            y += progressPanelHeight + padding;
            SetWindowPos(g_ctrls.btnPanel, NULL, padding, y, panelWidth, btnPanelHeight, SWP_NOZORDER);
            
            SetWindowPos(g_ctrls.reloadBtn, NULL, padding, y + 12, 100, 35, SWP_NOZORDER);
            SetWindowPos(g_ctrls.playBtn, NULL, padding + 120, y + 12, 100, 35, SWP_NOZORDER);
            SetWindowPos(g_ctrls.pauseBtn, NULL, padding + 240, y + 12, 100, 35, SWP_NOZORDER);
            SetWindowPos(g_ctrls.stopBtn, NULL, padding + 360, y + 12, 100, 35, SWP_NOZORDER);
            
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }

        case WM_TIMER:
            UpdateStatus(g_hWnd);
            SetControlState(g_hWnd);
            break;

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case IDC_PLAY_BTN: {
                    char buf[256];
                    GetWindowTextA(GetDlgItem(hWnd, IDC_BPM_EDIT), buf, 256);
                    g_player.bpm = atoi(buf);

                    GetWindowTextA(GetDlgItem(hWnd, IDC_CALIB_EDIT), buf, 256);
                    g_player.calibration = atoi(buf);

                    int len = GetWindowTextLengthA(GetDlgItem(hWnd, IDC_TRACK_EDIT));
                    if (len > 0) {
                        std::string track(len + 1, '\0');
                        GetWindowTextA(GetDlgItem(hWnd, IDC_TRACK_EDIT), &track[0], len + 1);
                        g_player.trackKeys = track;
                    }

                    g_player.Initialize();
                    g_player.Start();
                    SetControlState(hWnd);
                    UpdateStatus(hWnd);
                    break;
                }

                case IDC_PAUSE_BTN:
                    g_player.Pause();
                    SetControlState(hWnd);
                    break;

                case IDC_STOP_BTN:
                    g_player.Stop();
                    SetControlState(hWnd);
                    UpdateStatus(hWnd);
                    break;

                case IDC_RELOAD_BTN: {
                    char buf[256];
                    GetWindowTextA(GetDlgItem(hWnd, IDC_BPM_EDIT), buf, 256);
                    g_player.bpm = atoi(buf);

                    GetWindowTextA(GetDlgItem(hWnd, IDC_CALIB_EDIT), buf, 256);
                    g_player.calibration = atoi(buf);

                    int len = GetWindowTextLengthA(GetDlgItem(hWnd, IDC_TRACK_EDIT));
                    if (len > 0) {
                        std::string track(len + 1, '\0');
                        GetWindowTextA(GetDlgItem(hWnd, IDC_TRACK_EDIT), &track[0], len + 1);
                        g_player.trackKeys = track;
                    }

                    g_player.Initialize();
                    UpdateStatus(hWnd);
                    break;
                }

                case IDC_REFRESH_TREE:
                    RefreshWindowTree(GetDlgItem(hWnd, IDC_WINDOW_TREE));
                    break;

                case IDC_SELECT_WINDOW: {
                    HWND hTree = GetDlgItem(hWnd, IDC_WINDOW_TREE);
                    HTREEITEM hItem = TreeView_GetSelection(hTree);
                    if (hItem && g_treeItemMap.count(hItem)) {
                        g_player.hwnd.handle = g_treeItemMap[hItem];
                        
                        char className[256];
                        char title[256];
                        GetClassNameA(g_player.hwnd.handle, className, 256);
                        GetWindowTextA(g_player.hwnd.handle, title, 256);
                        std::string info = "已选择: " + std::string(title) + " (" + std::string(className) + ")";
                        MessageBoxA(hWnd, info.c_str(), "窗口选择", MB_OK);
                    } else {
                        MessageBoxA(hWnd, "请先在树中选择一个窗口", "提示", MB_OK);
                    }
                    break;
                }

                case IDC_IMPORT_BTN: {
                    OPENFILENAMEA ofn;
                    char szFile[260] = {0};
                    ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
                    ofn.lStructSize = sizeof(OPENFILENAMEA);
                    ofn.hwndOwner = hWnd;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrFilter = "文本文件 (*.txt)\0*.txt\0所有文件 (*.*)\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                    if (GetOpenFileNameA(&ofn) == TRUE) {
                        if (g_player.LoadTrackFromFile(szFile)) {
                            UpdateTrackEditor(hWnd);
                            UpdateStatus(hWnd);
                            MessageBoxA(hWnd, "谱子导入成功!", "成功", MB_OK);
                        } else {
                            MessageBoxA(hWnd, "谱子导入失败!", "错误", MB_OK | MB_ICONERROR);
                        }
                    }
                    break;
                }
            }
            break;
        }

        case WM_NOTIFY: {
            break;
        }

        case WM_HSCROLL: {
            WORD notifyCode = LOWORD(wParam);
            HWND hCtrl = (HWND)lParam;
            if (hCtrl == GetDlgItem(hWnd, IDC_PROGRESS_BAR) && (notifyCode == TB_ENDTRACK || notifyCode == TB_THUMBTRACK)) {
                int newPos = SendMessage(hCtrl, TBM_GETPOS, 0, 0);
                g_player.currentRow = newPos;
                UpdateStatus(hWnd);
            }
            break;
        }

        case WM_DESTROY:
            g_player.Stop();
            DeleteFonts();
            KillTimer(hWnd, 1);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"MusicPlayerClass";

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_TREEVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClassW(&wc);

    g_hWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"音乐演奏器",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 720,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!g_hWnd) return 0;

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}