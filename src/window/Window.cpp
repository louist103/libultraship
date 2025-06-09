#include "window/Window.h"
#include <string>
#include <fstream>
#include <iostream>
#include "public/bridge/consolevariablebridge.h"
#include "controller/controldevice/controller/mapping/keyboard/KeyboardScancodes.h"
#include "Context.h"
#include "controller/controldeck/ControlDeck.h"

#ifdef __APPLE__
#include "utils/AppleFolderManager.h"
#endif

namespace Ship {

Window::Window(std::shared_ptr<Gui> gui) {
    mGui = gui;
    mAvailableRenderers = std::make_shared<std::vector<int>>();
    mConfig = Context::GetInstance()->GetConfig();
}

Window::Window(std::vector<std::shared_ptr<GuiWindow>> guiWindows) : Window(std::make_shared<Gui>(guiWindows)) {
}

Window::Window() : Window(std::vector<std::shared_ptr<GuiWindow>>()) {
}

Window::~Window() {
    mGui->ShutDownImGui(this);
    SPDLOG_DEBUG("destruct window");
}

void Window::ToggleFullscreen() {
    SetFullscreen(!IsFullscreen());
}

float Window::GetCurrentAspectRatio() {
    return (float)GetWidth() / (float)GetHeight();
}

int32_t Window::GetLastScancode() {
    return mLastScancode;
}

void Window::SetLastScancode(int32_t scanCode) {
    mLastScancode = scanCode;
}

std::shared_ptr<Gui> Window::GetGui() {
    return mGui;
}

void Window::SaveWindowToConfig() {
    // This accepts conf in because it can be run in the destruction of LUS.
    mConfig->SetBool("Window.Fullscreen.Enabled", IsFullscreen());
    if (IsFullscreen()) {
        mConfig->SetInt("Window.Fullscreen.Width", (int32_t)GetWidth());
        mConfig->SetInt("Window.Fullscreen.Height", (int32_t)GetHeight());
    } else {
        mConfig->SetInt("Window.Width", (int32_t)GetWidth());
        mConfig->SetInt("Window.Height", (int32_t)GetHeight());
        mConfig->SetInt("Window.PositionX", GetPosX());
        mConfig->SetInt("Window.PositionY", GetPosY());
    }
}

int Window::GetRendererID() {
    return mRendererID;
}

std::shared_ptr<std::vector<int>> Window::GetAvailableRenderers() {
    return mAvailableRenderers;
}

bool Window::IsAvailableRenderer(int32_t rendererId) {
    // Verify the backend is available
    return std::find(mAvailableRenderers->begin(), mAvailableRenderers->end(), rendererId) !=
           mAvailableRenderers->end();
}

bool Window::ShouldForceCursorVisibility() {
    return mForceCursorVisibility;
}

void Window::SetForceCursorVisibility(bool visible) {
    mForceCursorVisibility = visible;
    CVarSetInteger("gForceCursorVisibility", visible);
    CVarSave();
}

void Window::SetRenderer(int renderer) {
    mRendererID = renderer;
    Context::GetInstance()->GetConfig()->SetRenderer(GetRendererID());
    Context::GetInstance()->GetConfig()->Save();
}

void Window::AddAvailableWindowBackend(int renderer) {
    mAvailableRenderers->push_back(renderer);
}
} // namespace Ship
