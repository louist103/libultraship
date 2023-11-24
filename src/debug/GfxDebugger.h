#pragma once

#include "libultraship/libultra/gbi.h"
#include <string>
#include "graphic/Fast3D/gfx_pc.h"
#include <set>

namespace LUS {

class GfxDebugger {
  public:
    void RequestDebugging();
    bool IsDebugging() const;
    bool IsDebuggingRequested() const;

    void DebugDisplayList(Gfx* cmds);

    void ResumeGame();

    const Gfx* GetDisplayList() const;

    const GfxPath& GetBreakPoint() const;

    bool HasBreakPoint(const GfxPath& path) const;

    void SetBreakPoint(const GfxPath& bp);

    bool HasBreakOnDlist(const std::string& dlistName);
    void AddBreakOnDlist(const std::string& dlistName);
    void RemoveBreakOnDlist(const std::string& dlistName);
    const std::set<std::string>& GetBreakOnDlists() const;

  private:
    bool mIsDebugging = false;
    bool mIsDebuggingRequested = false;
    Gfx* mDlist = nullptr;
    GfxPath mBreakPoint = {};
    std::set<std::string> mBreakOnDLs = {};
};

} // namespace LUS
