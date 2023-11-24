#pragma once

#include "window/gui/GuiWindow.h"
#include "libultraship/libultra/gbi.h"
#include <vector>
#include <graphic/Fast3D/gfx_pc.h>

namespace LUS {

class GfxDebuggerWindow : public GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    ~GfxDebuggerWindow();

  protected:
    void InitElement() override;
    void UpdateElement() override;
    void DrawElement() override;

  private:
    void DrawDisasNode(const Gfx* cmd, GfxPath& gfx_path) const;
    void DrawDisas();

  private:
    GfxPath mLastBreakPoint = {};
    char mPopupInpuBuf[256] = { 0 };
};

} // namespace LUS
