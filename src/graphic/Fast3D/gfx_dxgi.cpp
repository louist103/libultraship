#if defined(ENABLE_DX11) || defined(ENABLE_DX12)

#include <stdint.h>
#include <math.h>

#include <map>
#include <set>
#include <string>

#include <windows.h>
#include <windowsx.h> // GET_X_LPARAM(), GET_Y_LPARAM()
#include <wrl/client.h>
#include <dxgi1_3.h>
#include <dxgi1_4.h>
#include <dxgi1_5.h>
#include <versionhelpers.h>
#include <d3d11.h>

#include <shellscalingapi.h>

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif

#include "config/ConsoleVariable.h"
#include "config/Config.h"
#include "Context.h"

#include "gfx_window_manager_api.h"
#include "gfx_rendering_api.h"
#include "gfx_direct3d_common.h"
#include "gfx_screen_config.h"
#include "interpreter.h"

#define DECLARE_GFX_DXGI_FUNCTIONS
#include "gfx_dxgi.h"

#define WINCLASS_NAME L"N64GAME"
#define GFX_BACKEND_NAME "DXGI"

#define FRAME_INTERVAL_NS_NUMERATOR 1000000000
#define FRAME_INTERVAL_NS_DENOMINATOR (dxgi.target_fps)

#define NANOSECOND_IN_SECOND 1000000000
#define _100NANOSECONDS_IN_SECOND 10000000

#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC ((unsigned short)0x01)
#endif
#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE ((unsigned short)0x02)
#endif
using QWORD = uint64_t; // For NEXTRAWINPUTBLOCK

using namespace Microsoft::WRL; // For ComPtr

static struct {
    HWND h_wnd;

    // These four only apply in windowed mode.
    uint32_t current_width, current_height; // Width and height of client areas
    int32_t posX, posY;                     // Screen coordinates

    std::string game_name;
    bool is_running = true;

    HMODULE dxgi_module;
    HRESULT(__stdcall* CreateDXGIFactory1)(REFIID riid, void** factory);
    HRESULT(__stdcall* CreateDXGIFactory2)(UINT flags, REFIID iid, void** factory);

    bool process_dpi_awareness_done;

    bool is_full_screen, last_maximized_state;

    bool dxgi1_4;
    ComPtr<IDXGIFactory2> factory;
    ComPtr<IDXGISwapChain1> swap_chain;
    HANDLE waitable_object;
    ComPtr<IUnknown> swap_chain_device; // D3D11 Device or D3D12 Command Queue
    std::function<void()> before_destroy_swap_chain_fn;
    uint64_t qpc_init, qpc_freq;
    uint64_t frame_timestamp; // in units of 1/FRAME_INTERVAL_NS_DENOMINATOR nanoseconds
    std::map<UINT, DXGI_FRAME_STATISTICS> frame_stats;
    std::set<std::pair<UINT, UINT>> pending_frame_stats;
    bool dropped_frame;
    std::tuple<HMONITOR, RECT, BOOL> h_Monitor; // 0: Handle, 1: Display Monitor Rect, 2: Is_Primary
    std::vector<std::tuple<HMONITOR, RECT, BOOL>> monitor_list;
    bool zero_latency;
    double detected_hz;
    double display_period; // (1000 / dxgi.detected_hz) in ms
    UINT length_in_vsync_frames;
    uint32_t target_fps;
    uint32_t maximum_frame_latency;
    uint32_t applied_maximum_frame_latency;
    HANDLE timer;
    bool use_timer;
    bool tearing_support;
    bool is_vsync_enabled;
    bool mouse_pressed[5];
    float mouse_wheel[2];
    LARGE_INTEGER previous_present_time;
    bool is_mouse_captured;
    bool is_mouse_hovered;
    bool in_focus;
    bool has_mouse_position;
    RAWINPUTDEVICE raw_input_device[1];
    POINT raw_mouse_delta_buf;
    POINT prev_mouse_cursor_pos;

    void (*on_fullscreen_changed)(bool is_now_fullscreen);
    bool (*on_key_down)(int scancode);
    bool (*on_key_up)(int scancode);
    void (*on_all_keys_up)(void);
    bool (*on_mouse_button_down)(int btn);
    bool (*on_mouse_button_up)(int btn);
} dxgi;

static void load_dxgi_library() {
    dxgi.dxgi_module = LoadLibraryW(L"dxgi.dll");
    *(FARPROC*)&dxgi.CreateDXGIFactory1 = GetProcAddress(dxgi.dxgi_module, "CreateDXGIFactory1");
    *(FARPROC*)&dxgi.CreateDXGIFactory2 = GetProcAddress(dxgi.dxgi_module, "CreateDXGIFactory2");
}

static void apply_maximum_frame_latency(bool first) {
    DXGI_SWAP_CHAIN_DESC swap_desc = {};
    dxgi.swap_chain->GetDesc(&swap_desc);

    ComPtr<IDXGISwapChain2> swap_chain2;
    if ((swap_desc.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) &&
        dxgi.swap_chain->QueryInterface(__uuidof(IDXGISwapChain2), &swap_chain2) == S_OK) {
        ThrowIfFailed(swap_chain2->SetMaximumFrameLatency(dxgi.maximum_frame_latency));
        if (first) {
            dxgi.waitable_object = swap_chain2->GetFrameLatencyWaitableObject();
            WaitForSingleObject(dxgi.waitable_object, INFINITE);
        }
    } else {
        ComPtr<IDXGIDevice1> device1;
        ThrowIfFailed(dxgi.swap_chain->GetDevice(__uuidof(IDXGIDevice1), &device1));
        ThrowIfFailed(device1->SetMaximumFrameLatency(dxgi.maximum_frame_latency));
    }
    dxgi.applied_maximum_frame_latency = dxgi.maximum_frame_latency;
}

std::vector<std::tuple<HMONITOR, RECT, BOOL>> GetMonitorList() {
    std::vector<std::tuple<HMONITOR, RECT, BOOL>> monitors;
    EnumDisplayMonitors(
        nullptr, nullptr,
        [](HMONITOR hmon, HDC hdc, LPRECT rc, LPARAM lp) {
            UNREFERENCED_PARAMETER(hdc);
            UNREFERENCED_PARAMETER(rc);

            bool isPrimary;
            MONITORINFOEX mi = {};
            mi.cbSize = sizeof(MONITORINFOEX);
            GetMonitorInfo(hmon, &mi);
            auto monitors = (std::vector<std::tuple<HMONITOR, RECT, BOOL>>*)lp;
            if (mi.dwFlags == MONITORINFOF_PRIMARY) {
                isPrimary = TRUE;
            } else {
                isPrimary = FALSE;
            }
            monitors->push_back({ hmon, mi.rcMonitor, isPrimary });
            return TRUE;
        },
        (LPARAM)&monitors);
    return monitors;
}

// Uses coordinates to get a Monitor handle from a list
bool GetMonitorAtCoords(std::vector<std::tuple<HMONITOR, RECT, BOOL>> MonitorList, int x, int y, UINT cx, UINT cy,
                        std::tuple<HMONITOR, RECT, BOOL>& MonitorInfo) {
    RECT wr = { x, y, (x + cx), (y + cy) };
    std::tuple<HMONITOR, RECT, BOOL> primary;
    for (std::tuple<HMONITOR, RECT, BOOL> i : MonitorList) {
        if (PtInRect(&get<1>(i), POINT((x + (cx / 2)), (y + (cy / 2))))) {
            MonitorInfo = i;
            return true;
        }
        if (get<2>(i)) {
            primary = i;
        }
    }
    RECT intersection;
    LONG area;
    LONG lastArea = 0;
    std::tuple<HMONITOR, RECT, BOOL> biggest;
    for (std::tuple<HMONITOR, RECT, BOOL> i : MonitorList) {
        if (IntersectRect(&intersection, &get<1>(i), &wr)) {
            area = (intersection.right - intersection.left) * (intersection.bottom - intersection.top);
            if (area > lastArea) {
                lastArea = area;
                biggest = i;
            }
        }
    }
    if (lastArea > 0) {
        MonitorInfo = biggest;
        return true;
    }
    MonitorInfo = primary; // Fallback to primary, when out of bounds.
    return false;
}

static void toggle_borderless_window_full_screen(bool enable, bool call_callback) {
    // Windows 7 + flip mode + waitable object can't go to exclusive fullscreen,
    // so do borderless instead. If DWM is enabled, this means we get one monitor
    // sync interval of latency extra. On Win 10 however (maybe Win 8 too), due to
    // "fullscreen optimizations" the latency is eliminated.

    if (enable == dxgi.is_full_screen) {
        return;
    }

    if (!enable) {
        // Set in window mode with the last saved position and size
        SetWindowLongPtr(dxgi.h_wnd, GWL_STYLE, WS_VISIBLE | WS_OVERLAPPEDWINDOW);

        if (dxgi.last_maximized_state) {
            SetWindowPos(dxgi.h_wnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
            ShowWindow(dxgi.h_wnd, SW_MAXIMIZE);
        } else {
            std::tuple<HMONITOR, RECT, BOOL> Monitor;
            auto conf = Ship::Context::GetInstance()->GetConfig();
            dxgi.current_width = conf->GetInt("Window.Width", 640);
            dxgi.current_height = conf->GetInt("Window.Height", 480);
            dxgi.posX = conf->GetInt("Window.PositionX", 100);
            dxgi.posY = conf->GetInt("Window.PositionY", 100);
            if (!GetMonitorAtCoords(dxgi.monitor_list, dxgi.posX, dxgi.posY, dxgi.current_width, dxgi.current_height,
                                    Monitor)) { // Fallback to default when out of bounds.
                dxgi.posX = 100;
                dxgi.posY = 100;
            }
            RECT wr = { dxgi.posX, dxgi.posY, dxgi.posX + static_cast<int32_t>(dxgi.current_width),
                        dxgi.posY + static_cast<int32_t>(dxgi.current_height) };
            AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
            SetWindowPos(dxgi.h_wnd, NULL, wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top, SWP_FRAMECHANGED);
            ShowWindow(dxgi.h_wnd, SW_RESTORE);
        }

        dxgi.is_full_screen = false;
    } else {
        // Save if window is maximized or not
        WINDOWPLACEMENT window_placement;
        window_placement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(dxgi.h_wnd, &window_placement);
        dxgi.last_maximized_state = window_placement.showCmd == SW_SHOWMAXIMIZED;

        // We already know on what monitor we are (gets it on init or move)
        // Get info from that monitor
        RECT r = get<1>(dxgi.h_Monitor);

        // Set borderless full screen to that monitor
        SetWindowLongPtr(dxgi.h_wnd, GWL_STYLE, WS_VISIBLE | WS_POPUP);
        // OTRTODO: This should be setting the resolution from config.
        dxgi.current_width = r.right - r.left;
        dxgi.current_height = r.bottom - r.top;
        SetWindowPos(dxgi.h_wnd, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_FRAMECHANGED);
        dxgi.is_full_screen = true;
    }

    if (dxgi.on_fullscreen_changed != nullptr && call_callback) {
        dxgi.on_fullscreen_changed(enable);
    }
}

static void onkeydown(WPARAM w_param, LPARAM l_param) {
    int key = ((l_param >> 16) & 0x1ff);
    if (dxgi.on_key_down != nullptr) {
        dxgi.on_key_down(key);
    }
}
static void onkeyup(WPARAM w_param, LPARAM l_param) {
    int key = ((l_param >> 16) & 0x1ff);
    if (dxgi.on_key_up != nullptr) {
        dxgi.on_key_up(key);
    }
}

static void on_mouse_button_down(int btn) {
    if (!(btn >= 0 && btn < 5)) {
        return;
    }
    dxgi.mouse_pressed[btn] = true;
    if (dxgi.on_mouse_button_down != nullptr) {
        dxgi.on_mouse_button_down(btn);
    }
}
static void on_mouse_button_up(int btn) {
    dxgi.mouse_pressed[btn] = false;
    if (dxgi.on_mouse_button_up != nullptr) {
        dxgi.on_mouse_button_up(btn);
    }
}

double HzToPeriod(double Frequency) {
    if (Frequency == 0)
        Frequency = 60; // Default to 60, to prevent devision by zero
    double period = (double)1000 / Frequency;
    if (period == 0)
        period = 16.666666; // In case we go too low, use 16 ms (60 Hz) to prevent division by zero later
    return period;
}

void GetMonitorHzPeriod(HMONITOR hMonitor, double& Frequency, double& Period) {
    DEVMODE dm = {};
    dm.dmSize = sizeof(DEVMODE);
    if (hMonitor != NULL) {
        MONITORINFOEX minfoex = {};
        minfoex.cbSize = sizeof(MONITORINFOEX);

        if (GetMonitorInfo(hMonitor, (LPMONITORINFOEX)&minfoex)) {
            if (EnumDisplaySettings(minfoex.szDevice, ENUM_CURRENT_SETTINGS, &dm)) {
                Frequency = dm.dmDisplayFrequency;
                Period = HzToPeriod(Frequency);
            }
        }
    }
}

void GetMonitorHzPeriod(std::tuple<HMONITOR, RECT, BOOL> Monitor, double& Frequency, double& Period) {
    HMONITOR hMonitor = get<0>(Monitor);
    DEVMODE dm = {};
    dm.dmSize = sizeof(DEVMODE);
    if (hMonitor != NULL) {
        MONITORINFOEX minfoex = {};
        minfoex.cbSize = sizeof(MONITORINFOEX);

        if (GetMonitorInfo(hMonitor, (LPMONITORINFOEX)&minfoex)) {
            if (EnumDisplaySettings(minfoex.szDevice, ENUM_CURRENT_SETTINGS, &dm)) {
                Frequency = dm.dmDisplayFrequency;
                Period = HzToPeriod(Frequency);
            }
        }
    }
}

static void gfx_dxgi_close() {
    dxgi.is_running = false;
}

static void apply_mouse_capture_clip() {
    RECT rect;
    rect.left = dxgi.posX + 1;
    rect.top = dxgi.posY + 1;
    rect.right = dxgi.posX + dxgi.current_width - 1;
    rect.bottom = dxgi.posY + dxgi.current_height - 1;
    ClipCursor(&rect);
}

static void update_mouse_prev_pos() {
    if (!dxgi.has_mouse_position && dxgi.is_mouse_hovered && !dxgi.is_mouse_captured) {
        dxgi.has_mouse_position = true;

        int32_t x, y;
        gfx_dxgi_get_mouse_pos(&x, &y);
        dxgi.prev_mouse_cursor_pos.x = x;
        dxgi.prev_mouse_cursor_pos.y = y;
    }
}

void gfx_dxgi_handle_raw_input_buffered() {
    static UINT offset = -1;
    if (offset == -1) {
        offset = sizeof(RAWINPUTHEADER);

        BOOL isWow64;
        if (IsWow64Process(GetCurrentProcess(), &isWow64) && isWow64) {
            offset += 8;
        }
    }

    static BYTE* buf = NULL;
    static size_t bufsize;
    static const UINT RAWINPUT_BUFFER_SIZE_INCREMENT = 48 * 4; // 4 64-bit raw mouse packets

    while (true) {
        RAWINPUT* input = (RAWINPUT*)buf;
        UINT size = bufsize;
        UINT count = GetRawInputBuffer(input, &size, sizeof(RAWINPUTHEADER));

        if (!buf || (count == -1 && GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
            // realloc
            BYTE* newbuf = (BYTE*)std::realloc(buf, bufsize + RAWINPUT_BUFFER_SIZE_INCREMENT);
            if (!newbuf) {
                break;
            }
            buf = newbuf;
            bufsize += RAWINPUT_BUFFER_SIZE_INCREMENT;
            input = (RAWINPUT*)newbuf;
        } else if (count == -1) {
            // unhandled error
            DWORD err = GetLastError();
            fprintf(stderr, "Error: %lu\n", err);
        } else if (count == 0) {
            // there are no events
            break;
        } else {
            if (dxgi.is_mouse_captured && dxgi.in_focus) {
                while (count--) {
                    if (input->header.dwType == RIM_TYPEMOUSE) {
                        RAWMOUSE* rawmouse = (RAWMOUSE*)((BYTE*)input + offset);
                        dxgi.raw_mouse_delta_buf.x += rawmouse->lLastX;
                        dxgi.raw_mouse_delta_buf.y += rawmouse->lLastY;
                    }
                    input = NEXTRAWINPUTBLOCK(input);
                }
            }
        }
    }
}

static LRESULT CALLBACK gfx_dxgi_wnd_proc(HWND h_wnd, UINT message, WPARAM w_param, LPARAM l_param) {
    char fileName[256];
    Ship::WindowEvent event_impl;
    event_impl.Win32 = { h_wnd, static_cast<int>(message), static_cast<int>(w_param), static_cast<int>(l_param) };
    Ship::Context::GetInstance()->GetWindow()->GetGui()->HandleWindowEvents(event_impl);
    std::tuple<HMONITOR, RECT, BOOL> newMonitor;
    switch (message) {
        case WM_SIZE:
            dxgi.current_width = LOWORD(l_param);
            dxgi.current_height = HIWORD(l_param);
            GetMonitorAtCoords(dxgi.monitor_list, dxgi.posX, dxgi.posY, dxgi.current_width, dxgi.current_height,
                               newMonitor);
            if (get<0>(newMonitor) != get<0>(dxgi.h_Monitor)) {
                dxgi.h_Monitor = newMonitor;
                GetMonitorHzPeriod(dxgi.h_Monitor, dxgi.detected_hz, dxgi.display_period);
            }
            break;
        case WM_MOVE:
            dxgi.posX = GET_X_LPARAM(l_param);
            dxgi.posY = GET_Y_LPARAM(l_param);
            GetMonitorAtCoords(dxgi.monitor_list, dxgi.posX, dxgi.posY, dxgi.current_width, dxgi.current_height,
                               newMonitor);
            if (get<0>(newMonitor) != get<0>(dxgi.h_Monitor)) {
                dxgi.h_Monitor = newMonitor;
                GetMonitorHzPeriod(dxgi.h_Monitor, dxgi.detected_hz, dxgi.display_period);
            }
            break;
        case WM_CLOSE:
            gfx_dxgi_close();
            break;
        case WM_DPICHANGED: {
            RECT* const prcNewWindow = (RECT*)l_param;
            SetWindowPos(h_wnd, NULL, prcNewWindow->left, prcNewWindow->top, prcNewWindow->right - prcNewWindow->left,
                         prcNewWindow->bottom - prcNewWindow->top, SWP_NOZORDER | SWP_NOACTIVATE);
            dxgi.posX = prcNewWindow->left;
            dxgi.posY = prcNewWindow->top;
            dxgi.current_width = prcNewWindow->right - prcNewWindow->left;
            dxgi.current_height = prcNewWindow->bottom - prcNewWindow->top;
            break;
        }
        case WM_ENDSESSION:
            // This hopefully gives the game a chance to shut down, before windows kills it.
            if (w_param == TRUE) {
                gfx_dxgi_close();
            }
            break;
        case WM_ACTIVATEAPP:
            if (dxgi.on_all_keys_up != nullptr) {
                dxgi.on_all_keys_up();
            }
            break;
        case WM_KEYDOWN:
            onkeydown(w_param, l_param);
            break;
        case WM_KEYUP:
            onkeyup(w_param, l_param);
            break;
        case WM_LBUTTONDOWN:
            on_mouse_button_down(0);
            break;
        case WM_LBUTTONUP:
            on_mouse_button_up(0);
            break;
        case WM_MBUTTONDOWN:
            on_mouse_button_down(1);
            break;
        case WM_MBUTTONUP:
            on_mouse_button_up(1);
            break;
        case WM_RBUTTONDOWN:
            on_mouse_button_down(2);
            break;
        case WM_RBUTTONUP:
            on_mouse_button_up(2);
            break;
        case WM_XBUTTONDOWN: {
            int btn = 2 + GET_XBUTTON_WPARAM(w_param);
            on_mouse_button_down(btn);
            break;
        }
        case WM_XBUTTONUP: {
            int btn = 2 + GET_XBUTTON_WPARAM(w_param);
            on_mouse_button_up(btn);
            break;
        }
        case WM_MOUSEHWHEEL:
            dxgi.mouse_wheel[0] = GET_WHEEL_DELTA_WPARAM(w_param) / WHEEL_DELTA;
            break;
        case WM_MOUSEWHEEL:
            dxgi.mouse_wheel[1] = GET_WHEEL_DELTA_WPARAM(w_param) / WHEEL_DELTA;
            break;
        case WM_INPUT: {
            // At this point the top most message should already be off the queue.
            // So we don't need to get it all, if mouse isn't captured.
            if (dxgi.is_mouse_captured && dxgi.in_focus) {
                uint32_t size = sizeof(RAWINPUT);
                static RAWINPUT raw[sizeof(RAWINPUT)];
                GetRawInputData((HRAWINPUT)l_param, RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER));

                if (raw->header.dwType == RIM_TYPEMOUSE) {
                    dxgi.raw_mouse_delta_buf.x += raw->data.mouse.lLastX;
                    dxgi.raw_mouse_delta_buf.y += raw->data.mouse.lLastY;
                }
            }
            // The rest still needs to use that, to get them off the queue.
            gfx_dxgi_handle_raw_input_buffered();
            break;
        }
        case WM_MOUSEMOVE:
            if (!dxgi.is_mouse_hovered) {
                dxgi.is_mouse_hovered = true;
                update_mouse_prev_pos();
            }
            break;
        case WM_MOUSELEAVE:
            dxgi.is_mouse_hovered = false;
            dxgi.has_mouse_position = false;
            break;
        case WM_DROPFILES:
            DragQueryFileA((HDROP)w_param, 0, fileName, 256);
            Ship::Context::GetInstance()->GetConsoleVariables()->SetString(CVAR_DROPPED_FILE, fileName);
            Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(CVAR_NEW_FILE_DROPPED, 1);
            Ship::Context::GetInstance()->GetConsoleVariables()->Save();

            break;
        case WM_DISPLAYCHANGE:
            dxgi.monitor_list = GetMonitorList();
            GetMonitorAtCoords(dxgi.monitor_list, dxgi.posX, dxgi.posY, dxgi.current_width, dxgi.current_height,
                               dxgi.h_Monitor);
            GetMonitorHzPeriod(dxgi.h_Monitor, dxgi.detected_hz, dxgi.display_period);
            break;
        case WM_SETFOCUS:
            dxgi.in_focus = true;
            if (dxgi.is_mouse_captured) {
                apply_mouse_capture_clip();
            }
            break;
        case WM_KILLFOCUS:
            dxgi.in_focus = false;
            break;
        default:
            return DefWindowProcW(h_wnd, message, w_param, l_param);
    }
    return 0;
}

static BOOL CALLBACK WIN_ResourceNameCallback(HMODULE hModule, LPCTSTR lpType, LPTSTR lpName, LONG_PTR lParam) {
    WNDCLASSEX* wcex = (WNDCLASSEX*)lParam;

    (void)lpType; /* We already know that the resource type is RT_GROUP_ICON. */

    /* We leave hIconSm as NULL as it will allow Windows to automatically
       choose the appropriate small icon size to suit the current DPI. */
    wcex->hIcon = LoadIcon(hModule, lpName);

    /* Do not bother enumerating any more. */
    return FALSE;
}

void gfx_dxgi_init(const char* game_name, const char* gfx_api_name, bool start_in_fullscreen, uint32_t width,
                   uint32_t height, int32_t posX, int32_t posY) {
    LARGE_INTEGER qpc_init, qpc_freq;
    QueryPerformanceCounter(&qpc_init);
    QueryPerformanceFrequency(&qpc_freq);
    dxgi.qpc_init = qpc_init.QuadPart;
    dxgi.qpc_freq = qpc_freq.QuadPart;

    dxgi.target_fps = 60;
    dxgi.maximum_frame_latency = 2;

    // Use high-resolution timer by default on Windows 10 (so that NtSetTimerResolution (...) hacks are not needed)
    dxgi.timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    // Fallback to low resolution timer if unsupported by the OS
    if (dxgi.timer == nullptr) {
        dxgi.timer = CreateWaitableTimer(nullptr, FALSE, nullptr);
    }

    // Prepare window title

    char title[512];
    wchar_t w_title[512];
    int len = sprintf(title, "%s (%s)", game_name, gfx_api_name);
    mbstowcs(w_title, title, len + 1);
    dxgi.game_name = game_name;

    // Create window
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = gfx_dxgi_wnd_proc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = GetModuleHandle(nullptr);
    wcex.hIcon = nullptr;
    wcex.hIconSm = nullptr;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = WINCLASS_NAME;
    wcex.hIconSm = nullptr;

    EnumResourceNames(wcex.hInstance, RT_GROUP_ICON, WIN_ResourceNameCallback, (LONG_PTR)&wcex);

    ATOM winclass = RegisterClassExW(&wcex);

    RECT wr = { 0, 0, width, height };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    dxgi.current_width = wr.right - wr.left;
    dxgi.current_height = wr.bottom - wr.top;
    dxgi.monitor_list = GetMonitorList();
    dxgi.posX = posX;
    dxgi.posY = posY;
    if (!GetMonitorAtCoords(dxgi.monitor_list, dxgi.posX, dxgi.posY, dxgi.current_width, dxgi.current_height,
                            dxgi.h_Monitor)) {
        dxgi.posX = 100;
        dxgi.posY = 100;
    }

    dxgi.h_wnd = CreateWindowW(WINCLASS_NAME, w_title, WS_OVERLAPPEDWINDOW, dxgi.posX + wr.left, dxgi.posY + wr.top,
                               dxgi.current_width, dxgi.current_height, nullptr, nullptr, nullptr, nullptr);

    load_dxgi_library();

    ShowWindow(dxgi.h_wnd, SW_SHOW);
    UpdateWindow(dxgi.h_wnd);

    // Get refresh rate
    GetMonitorHzPeriod(dxgi.h_Monitor, dxgi.detected_hz, dxgi.display_period);

    if (start_in_fullscreen) {
        toggle_borderless_window_full_screen(true, false);
    }

    DragAcceptFiles(dxgi.h_wnd, TRUE);

    // Mouse init
    dxgi.raw_input_device[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    dxgi.raw_input_device[0].usUsage = HID_USAGE_GENERIC_MOUSE;
    dxgi.raw_input_device[0].dwFlags = RIDEV_INPUTSINK;
    dxgi.raw_input_device[0].hwndTarget = dxgi.h_wnd;
    RegisterRawInputDevices(dxgi.raw_input_device, 1, sizeof(dxgi.raw_input_device[0]));
}

static void gfx_dxgi_set_fullscreen_changed_callback(void (*on_fullscreen_changed)(bool is_now_fullscreen)) {
    dxgi.on_fullscreen_changed = on_fullscreen_changed;
}

static void gfx_dxgi_set_cursor_visibility(bool visible) {
    // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showcursor
    // https://devblogs.microsoft.com/oldnewthing/20091217-00/?p=15643
    // ShowCursor uses a counter, not a boolean value, and increments or decrements that value when called
    // This means we need to keep calling it until we get the value we want

    //
    //  NOTE:  If you continue calling until you "get the value you want" and there is no mouse attached,
    //  it will lock the software up.  Windows always returns -1 if there is no mouse!
    //

    const int _MAX_TRIES = 15; // Prevent spinning infinitely if no mouse is plugged in

    int cursorVisibilityTries = 0;
    int cursorVisibilityCounter;
    if (visible) {
        do {
            cursorVisibilityCounter = ShowCursor(true);
        } while (cursorVisibilityCounter < 0 && ++cursorVisibilityTries < _MAX_TRIES);
    } else {
        do {
            cursorVisibilityCounter = ShowCursor(false);
        } while (cursorVisibilityCounter >= 0);
    }
}

static void gfx_dxgi_set_mouse_pos(int32_t x, int32_t y) {
    SetCursorPos(x, y);
}

static void gfx_dxgi_get_mouse_pos(int32_t* x, int32_t* y) {
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(dxgi.h_wnd, &p);
    *x = p.x;
    *y = p.y;
}

static void gfx_dxgi_get_mouse_delta(int32_t* x, int32_t* y) {
    if (dxgi.is_mouse_captured) {
        *x = dxgi.raw_mouse_delta_buf.x;
        *y = dxgi.raw_mouse_delta_buf.y;
        dxgi.raw_mouse_delta_buf.x = 0;
        dxgi.raw_mouse_delta_buf.y = 0;
    } else if (dxgi.has_mouse_position) {
        int32_t current_x, current_y;
        gfx_dxgi_get_mouse_pos(&current_x, &current_y);
        *x = current_x - dxgi.prev_mouse_cursor_pos.x;
        *y = current_y - dxgi.prev_mouse_cursor_pos.y;
        dxgi.prev_mouse_cursor_pos.x = current_x;
        dxgi.prev_mouse_cursor_pos.y = current_y;
    } else {
        *x = 0;
        *y = 0;
    }
}

static void gfx_dxgi_get_mouse_wheel(float* x, float* y) {
    *x = dxgi.mouse_wheel[0];
    *y = dxgi.mouse_wheel[1];
    dxgi.mouse_wheel[0] = 0;
    dxgi.mouse_wheel[1] = 0;
}

static bool gfx_dxgi_get_mouse_state(uint32_t btn) {
    return dxgi.mouse_pressed[btn];
}

static void gfx_dxgi_set_mouse_capture(bool capture) {
    dxgi.is_mouse_captured = capture;
    if (capture) {
        apply_mouse_capture_clip();
        gfx_dxgi_set_cursor_visibility(false);
        SetCapture(dxgi.h_wnd);
        dxgi.has_mouse_position = false;
    } else {
        ClipCursor(nullptr);
        gfx_dxgi_set_cursor_visibility(true);
        ReleaseCapture();
        update_mouse_prev_pos();
    }
}

static bool gfx_dxgi_is_mouse_captured() {
    return dxgi.is_mouse_captured;
}

static void gfx_dxgi_set_fullscreen(bool enable) {
    toggle_borderless_window_full_screen(enable, true);
}

static void gfx_dxgi_get_active_window_refresh_rate(uint32_t* refresh_rate) {
    *refresh_rate = (uint32_t)roundf(dxgi.detected_hz);
}

static void gfx_dxgi_set_keyboard_callbacks(bool (*on_key_down)(int scancode), bool (*on_key_up)(int scancode),
                                            void (*on_all_keys_up)()) {
    dxgi.on_key_down = on_key_down;
    dxgi.on_key_up = on_key_up;
    dxgi.on_all_keys_up = on_all_keys_up;
}

static void gfx_dxgi_set_mouse_callbacks(bool (*on_btn_down)(int btn), bool (*on_btn_up)(int btn)) {
    dxgi.on_mouse_button_down = on_btn_down;
    dxgi.on_mouse_button_up = on_btn_up;
}

static void gfx_dxgi_get_dimensions(uint32_t* width, uint32_t* height, int32_t* posX, int32_t* posY) {
    *width = dxgi.current_width;
    *height = dxgi.current_height;
    *posX = dxgi.posX;
    *posY = dxgi.posY;
}

static void gfx_dxgi_handle_events() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            dxgi.is_running = false;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

static uint64_t qpc_to_ns(uint64_t qpc) {
    return qpc / dxgi.qpc_freq * NANOSECOND_IN_SECOND + qpc % dxgi.qpc_freq * NANOSECOND_IN_SECOND / dxgi.qpc_freq;
}

static uint64_t qpc_to_100ns(uint64_t qpc) {
    return qpc / dxgi.qpc_freq * _100NANOSECONDS_IN_SECOND +
           qpc % dxgi.qpc_freq * _100NANOSECONDS_IN_SECOND / dxgi.qpc_freq;
}

static bool gfx_dxgi_is_frame_ready() {
    DXGI_FRAME_STATISTICS stats;
    if (dxgi.swap_chain->GetFrameStatistics(&stats) == S_OK &&
        (stats.SyncRefreshCount != 0 || stats.SyncQPCTime.QuadPart != 0ULL)) {
        {
            LARGE_INTEGER t0;
            QueryPerformanceCounter(&t0);
            // printf("Get frame stats: %llu\n", (unsigned long long)(t0.QuadPart - dxgi.qpc_init));
        }
        // printf("stats: %u %u %u %u %u %.6f\n", dxgi.pending_frame_stats.rbegin()->first,
        // dxgi.pending_frame_stats.rbegin()->second, stats.PresentCount, stats.PresentRefreshCount,
        // stats.SyncRefreshCount, (double)(stats.SyncQPCTime.QuadPart - dxgi.qpc_init) / dxgi.qpc_freq);
        if (dxgi.frame_stats.empty() || dxgi.frame_stats.rbegin()->second.PresentCount != stats.PresentCount) {
            dxgi.frame_stats.insert(std::make_pair(stats.PresentCount, stats));
        }
        if (dxgi.frame_stats.size() > 3) {
            dxgi.frame_stats.erase(dxgi.frame_stats.begin());
        }
    }
    if (!dxgi.frame_stats.empty()) {
        while (!dxgi.pending_frame_stats.empty() &&
               dxgi.pending_frame_stats.begin()->first < dxgi.frame_stats.rbegin()->first) {
            dxgi.pending_frame_stats.erase(dxgi.pending_frame_stats.begin());
        }
    }
    while (dxgi.pending_frame_stats.size() > 40) {
        // Just make sure the list doesn't grow too large if GetFrameStatistics fails.
        dxgi.pending_frame_stats.erase(dxgi.pending_frame_stats.begin());

        // These are not that useful anymore
        dxgi.frame_stats.clear();
    }

    dxgi.use_timer = false;

    dxgi.frame_timestamp += FRAME_INTERVAL_NS_NUMERATOR;

    if (dxgi.frame_stats.size() >= 2) {
        DXGI_FRAME_STATISTICS* first = &dxgi.frame_stats.begin()->second;
        DXGI_FRAME_STATISTICS* last = &dxgi.frame_stats.rbegin()->second;
        uint64_t sync_qpc_diff = last->SyncQPCTime.QuadPart - first->SyncQPCTime.QuadPart;
        UINT sync_vsync_diff = last->SyncRefreshCount - first->SyncRefreshCount;
        UINT present_vsync_diff = last->PresentRefreshCount - first->PresentRefreshCount;
        UINT present_diff = last->PresentCount - first->PresentCount;

        if (sync_vsync_diff == 0) {
            sync_vsync_diff = 1;
        }

        double estimated_vsync_interval = (double)sync_qpc_diff / (double)sync_vsync_diff;
        uint64_t estimated_vsync_interval_ns = qpc_to_ns(estimated_vsync_interval);
        // printf("Estimated vsync_interval: %d\n", (int)estimated_vsync_interval_ns);
        if (estimated_vsync_interval_ns < 2000 || estimated_vsync_interval_ns > 1000000000) {
            // Unreasonable, maybe a monitor change
            estimated_vsync_interval_ns = 16666666;
            estimated_vsync_interval = (double)estimated_vsync_interval_ns * dxgi.qpc_freq / 1000000000;
        }

        UINT queued_vsyncs = 0;
        bool is_first = true;
        for (const std::pair<UINT, UINT>& p : dxgi.pending_frame_stats) {
            /*if (is_first && dxgi.zero_latency) {
                is_first = false;
                continue;
            }*/
            queued_vsyncs += p.second;
        }

        uint64_t last_frame_present_end_qpc =
            (last->SyncQPCTime.QuadPart - dxgi.qpc_init) + estimated_vsync_interval * queued_vsyncs;
        uint64_t last_end_ns = qpc_to_ns(last_frame_present_end_qpc);

        double vsyncs_to_wait = (double)(int64_t)(dxgi.frame_timestamp / FRAME_INTERVAL_NS_DENOMINATOR - last_end_ns) /
                                estimated_vsync_interval_ns;
        // printf("ts: %llu, last_end_ns: %llu, Init v: %f\n", dxgi.frame_timestamp / 3, last_end_ns,
        // vsyncs_to_wait);

        if (vsyncs_to_wait <= 0) {
            // Too late

            if ((int64_t)(dxgi.frame_timestamp / FRAME_INTERVAL_NS_DENOMINATOR - last_end_ns) < -66666666) {
                // The application must have been paused or similar
                vsyncs_to_wait = round(((double)FRAME_INTERVAL_NS_NUMERATOR / FRAME_INTERVAL_NS_DENOMINATOR) /
                                       estimated_vsync_interval_ns);
                if (vsyncs_to_wait < 1) {
                    vsyncs_to_wait = 1;
                }
                dxgi.frame_timestamp =
                    FRAME_INTERVAL_NS_DENOMINATOR * (last_end_ns + vsyncs_to_wait * estimated_vsync_interval_ns);
            } else {
                // Drop frame
                // printf("Dropping frame\n");
                dxgi.dropped_frame = true;
                return false;
            }
        }
        double orig_wait = vsyncs_to_wait;
        if (floor(vsyncs_to_wait) != vsyncs_to_wait) {
            uint64_t left = last_end_ns + floor(vsyncs_to_wait) * estimated_vsync_interval_ns;
            uint64_t right = last_end_ns + ceil(vsyncs_to_wait) * estimated_vsync_interval_ns;
            uint64_t adjusted_desired_time =
                dxgi.frame_timestamp / FRAME_INTERVAL_NS_DENOMINATOR +
                (last_end_ns + (FRAME_INTERVAL_NS_NUMERATOR / FRAME_INTERVAL_NS_DENOMINATOR) >
                         dxgi.frame_timestamp / FRAME_INTERVAL_NS_DENOMINATOR
                     ? 2000000
                     : -2000000);
            int64_t diff_left = adjusted_desired_time - left;
            int64_t diff_right = right - adjusted_desired_time;
            if (diff_left < 0) {
                diff_left = -diff_left;
            }
            if (diff_right < 0) {
                diff_right = -diff_right;
            }
            if (diff_left < diff_right) {
                vsyncs_to_wait = floor(vsyncs_to_wait);
            } else {
                vsyncs_to_wait = ceil(vsyncs_to_wait);
            }
            if (vsyncs_to_wait == 0) {
                // printf("vsyncs_to_wait became 0 so dropping frame\n");
                dxgi.dropped_frame = true;
                return false;
            }
        }
        // printf("v: %d\n", (int)vsyncs_to_wait);
        if (vsyncs_to_wait > 4) {
            // Invalid, so use timer based solution
            vsyncs_to_wait = 4;
            dxgi.use_timer = true;
        }
    } else {
        dxgi.use_timer = true;
    }
    // dxgi.length_in_vsync_frames is used as present interval. Present interval >1 (aka fractional V-Sync)
    // breaks VRR and introduces even more input lag than capping via normal V-Sync does.
    // Get the present interval the user wants instead (V-Sync toggle).
    dxgi.is_vsync_enabled = Ship::Context::GetInstance()->GetConsoleVariables()->GetInteger(CVAR_VSYNC_ENABLED, 1);
    dxgi.length_in_vsync_frames = dxgi.is_vsync_enabled ? 1 : 0;
    return true;
}

static void gfx_dxgi_swap_buffers_begin() {
    LARGE_INTEGER t;
    dxgi.use_timer = true;
    if (dxgi.use_timer || (dxgi.tearing_support && !dxgi.is_vsync_enabled)) {
        ComPtr<ID3D11Device> device;
        dxgi.swap_chain_device.As(&device);

        if (device != nullptr) {
            ComPtr<ID3D11DeviceContext> dev_ctx;
            device->GetImmediateContext(&dev_ctx);

            if (dev_ctx != nullptr) {
                // Always flush the immediate context before forcing a CPU-wait, otherwise the GPU might only start
                // working when the SwapChain is presented.
                dev_ctx->Flush();
            }
        }
        QueryPerformanceCounter(&t);
        int64_t next = qpc_to_100ns(dxgi.previous_present_time.QuadPart) +
                       FRAME_INTERVAL_NS_NUMERATOR / (FRAME_INTERVAL_NS_DENOMINATOR * 100);
        int64_t left = next - qpc_to_100ns(t.QuadPart) - 15000UL;
        if (left > 0) {
            LARGE_INTEGER li;
            li.QuadPart = -left;
            SetWaitableTimer(dxgi.timer, &li, 0, nullptr, nullptr, false);
            WaitForSingleObject(dxgi.timer, INFINITE);
        }

        QueryPerformanceCounter(&t);
        t.QuadPart = qpc_to_100ns(t.QuadPart);
        while (t.QuadPart < next) {
            YieldProcessor();
            QueryPerformanceCounter(&t);
            t.QuadPart = qpc_to_100ns(t.QuadPart);
        }
    }
    QueryPerformanceCounter(&t);
    dxgi.previous_present_time = t;
    if (dxgi.tearing_support && !dxgi.length_in_vsync_frames) {
        // 512: DXGI_PRESENT_ALLOW_TEARING - allows for true V-Sync off with flip model
        ThrowIfFailed(dxgi.swap_chain->Present(dxgi.length_in_vsync_frames, DXGI_PRESENT_ALLOW_TEARING));
    } else {
        ThrowIfFailed(dxgi.swap_chain->Present(dxgi.length_in_vsync_frames, 0));
    }

    UINT this_present_id;
    if (dxgi.swap_chain->GetLastPresentCount(&this_present_id) == S_OK) {
        dxgi.pending_frame_stats.insert(std::make_pair(this_present_id, dxgi.length_in_vsync_frames));
    }
    dxgi.dropped_frame = false;
}

static void gfx_dxgi_swap_buffers_end() {
    LARGE_INTEGER t0, t1, t2;
    QueryPerformanceCounter(&t0);
    QueryPerformanceCounter(&t1);

    if (dxgi.applied_maximum_frame_latency > dxgi.maximum_frame_latency) {
        // If latency is decreased, you have to wait the same amout of times as the old latency was set to
        int times_to_wait = dxgi.applied_maximum_frame_latency;
        int latency = dxgi.maximum_frame_latency;
        dxgi.maximum_frame_latency = 1;
        apply_maximum_frame_latency(false);
        if (dxgi.waitable_object != nullptr) {
            while (times_to_wait > 0) {
                WaitForSingleObject(dxgi.waitable_object, INFINITE);
                times_to_wait--;
            }
        }
        dxgi.maximum_frame_latency = latency;
        apply_maximum_frame_latency(false);

        return; // Make sure we don't wait a second time on the waitable object, since that would hang the program
    } else if (dxgi.applied_maximum_frame_latency != dxgi.maximum_frame_latency) {
        apply_maximum_frame_latency(false);
    }

    if (!dxgi.dropped_frame) {
        if (dxgi.waitable_object != nullptr) {
            WaitForSingleObject(dxgi.waitable_object, INFINITE);
        }
        // else TODO: maybe sleep until some estimated time the frame will be shown to reduce lag
    }

    DXGI_FRAME_STATISTICS stats;
    dxgi.swap_chain->GetFrameStatistics(&stats);

    QueryPerformanceCounter(&t2);

    dxgi.zero_latency = dxgi.pending_frame_stats.rbegin()->first == stats.PresentCount;

    // printf(L"done %I64u gpu:%d wait:%d freed:%I64u frame:%u %u monitor:%u t:%I64u\n", (unsigned long
    // long)(t0.QuadPart - dxgi.qpc_init), (int)(t1.QuadPart - t0.QuadPart), (int)(t2.QuadPart - t0.QuadPart), (unsigned
    // long long)(t2.QuadPart - dxgi.qpc_init), dxgi.pending_frame_stats.rbegin()->first, stats.PresentCount,
    // stats.SyncRefreshCount, (unsigned long long)(stats.SyncQPCTime.QuadPart - dxgi.qpc_init));
}

static double gfx_dxgi_get_time() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)(t.QuadPart - dxgi.qpc_init) / dxgi.qpc_freq;
}

static void gfx_dxgi_set_target_fps(int fps) {
    uint32_t old_fps = dxgi.target_fps;
    uint64_t t0 = dxgi.frame_timestamp / old_fps;
    uint32_t t1 = dxgi.frame_timestamp % old_fps;
    dxgi.target_fps = fps;
    dxgi.frame_timestamp = t0 * dxgi.target_fps + t1 * dxgi.target_fps / old_fps;
}

static void gfx_dxgi_set_maximum_frame_latency(int latency) {
    dxgi.maximum_frame_latency = latency;
}

void gfx_dxgi_create_factory_and_device(bool debug, int d3d_version,
                                        bool (*create_device_fn)(IDXGIAdapter1* adapter, bool test_only)) {
    if (dxgi.CreateDXGIFactory2 != nullptr) {
        ThrowIfFailed(
            dxgi.CreateDXGIFactory2(debug ? DXGI_CREATE_FACTORY_DEBUG : 0, __uuidof(IDXGIFactory2), &dxgi.factory));
    } else {
        ThrowIfFailed(dxgi.CreateDXGIFactory1(__uuidof(IDXGIFactory2), &dxgi.factory));
    }

    ComPtr<IDXGIFactory4> factory4;
    if (dxgi.factory->QueryInterface(__uuidof(IDXGIFactory4), &factory4) == S_OK) {
        dxgi.dxgi1_4 = true;

        ComPtr<IDXGIFactory5> factory;
        HRESULT hr = dxgi.factory.As(&factory);
        BOOL allowTearing = FALSE;
        if (SUCCEEDED(hr)) {
            hr = factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
        }

        dxgi.tearing_support = SUCCEEDED(hr) && allowTearing;
    }

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; dxgi.factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & 2 /*DXGI_ADAPTER_FLAG_SOFTWARE*/) { // declaration missing in mingw headers
            continue;
        }
        if (create_device_fn(adapter.Get(), true)) {
            break;
        }
    }
    create_device_fn(adapter.Get(), false);
}

void gfx_dxgi_create_swap_chain(IUnknown* device, std::function<void()>&& before_destroy_fn) {
    bool win8 = IsWindows8OrGreater();                 // DXGI_SCALING_NONE is only supported on Win8 and beyond
    bool dxgi_13 = dxgi.CreateDXGIFactory2 != nullptr; // DXGI 1.3 introduced waitable object

    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
    swap_chain_desc.BufferCount = 3;
    swap_chain_desc.Width = 0;
    swap_chain_desc.Height = 0;
    swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.Scaling = win8 ? DXGI_SCALING_NONE : DXGI_SCALING_STRETCH;
    swap_chain_desc.SwapEffect =
        dxgi.dxgi1_4 ? DXGI_SWAP_EFFECT_FLIP_DISCARD : // Introduced in DXGI 1.4 and Windows 10
            DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; // Apparently flip sequential was also backported to Win 7 Platform Update
    swap_chain_desc.Flags = dxgi_13 ? DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT : 0;
    if (dxgi.tearing_support) {
        swap_chain_desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING; // Now we can use DXGI_PRESENT_ALLOW_TEARING
    }
    swap_chain_desc.SampleDesc.Count = 1;

    ThrowIfFailed(
        dxgi.factory->CreateSwapChainForHwnd(device, dxgi.h_wnd, &swap_chain_desc, nullptr, nullptr, &dxgi.swap_chain));
    ThrowIfFailed(dxgi.factory->MakeWindowAssociation(dxgi.h_wnd, DXGI_MWA_NO_ALT_ENTER));

    apply_maximum_frame_latency(true);

    ThrowIfFailed(dxgi.swap_chain->GetDesc1(&swap_chain_desc));

    dxgi.swap_chain_device = device;
    dxgi.before_destroy_swap_chain_fn = std::move(before_destroy_fn);
}

bool gfx_dxgi_is_running() {
    return dxgi.is_running;
}

HWND gfx_dxgi_get_h_wnd() {
    return dxgi.h_wnd;
}

IDXGISwapChain1* gfx_dxgi_get_swap_chain() {
    return dxgi.swap_chain.Get();
}

void ThrowIfFailed(HRESULT res) {
    if (FAILED(res)) {
        fprintf(stderr, "Error: 0x%08X\n", res);
        throw res;
    }
}

void ThrowIfFailed(HRESULT res, HWND h_wnd, const char* message) {
    if (FAILED(res)) {
        char full_message[256];
        sprintf(full_message, "%s\n\nHRESULT: 0x%08X", message, res);
        MessageBoxA(h_wnd, full_message, "Error", MB_OK | MB_ICONERROR);
        throw res;
    }
}

const char* gfx_dxgi_get_key_name(int scancode) {
    static char text[64];
    GetKeyNameTextA(scancode << 16, text, 64);
    return text;
}

bool gfx_dxgi_can_disable_vsync() {
    return dxgi.tearing_support;
}

void gfx_dxgi_destroy() {
    // TODO: destroy _any_ resources used by dxgi, including the window handle
}

bool gfx_dxgi_is_fullscreen() {
    return dxgi.is_full_screen;
}

extern "C" struct GfxWindowManagerAPI gfx_dxgi_api = { gfx_dxgi_init,
                                                       gfx_dxgi_close,
                                                       gfx_dxgi_set_keyboard_callbacks,
                                                       gfx_dxgi_set_mouse_callbacks,
                                                       gfx_dxgi_set_fullscreen_changed_callback,
                                                       gfx_dxgi_set_fullscreen,
                                                       gfx_dxgi_get_active_window_refresh_rate,
                                                       gfx_dxgi_set_cursor_visibility,
                                                       gfx_dxgi_set_mouse_pos,
                                                       gfx_dxgi_get_mouse_pos,
                                                       gfx_dxgi_get_mouse_delta,
                                                       gfx_dxgi_get_mouse_wheel,
                                                       gfx_dxgi_get_mouse_state,
                                                       gfx_dxgi_set_mouse_capture,
                                                       gfx_dxgi_is_mouse_captured,
                                                       gfx_dxgi_get_dimensions,
                                                       gfx_dxgi_handle_events,
                                                       gfx_dxgi_is_frame_ready,
                                                       gfx_dxgi_swap_buffers_begin,
                                                       gfx_dxgi_swap_buffers_end,
                                                       gfx_dxgi_get_time,
                                                       gfx_dxgi_set_target_fps,
                                                       gfx_dxgi_set_maximum_frame_latency,
                                                       gfx_dxgi_get_key_name,
                                                       gfx_dxgi_can_disable_vsync,
                                                       gfx_dxgi_is_running,
                                                       gfx_dxgi_destroy,
                                                       gfx_dxgi_is_fullscreen };

#endif
