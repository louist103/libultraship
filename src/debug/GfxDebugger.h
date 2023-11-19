#pragma once

#include "libultraship/libultra/gbi.h"
#include <string>
#include <vector>

namespace LUS {

class GfxDebugger {
  public:
    void RequestDebugging() {
        mIsDebuggingRequested = true;
    }
    bool IsDebugging() const {
        return mIsDebugging;
    }
    bool IsDebuggingRequested() const {
        return mIsDebuggingRequested;
    }

    void DebugDisplayList(Gfx* cmds);

    void ResumeGame() {
        mIsDebugging = false;
        mIsDebuggingRequested = false;
        mDlist = nullptr;
    }

    const Gfx* GetDisplayList() const {
        return mDlist;
    }

    const std::vector<const Gfx*>& GetBreakPoint() const {
        return mBreakPoint;
    }

    bool HasBreakPoint(const std::vector<const Gfx*>& path) const;

    void SetBreakPoint(const std::vector<const Gfx*>& bp) {
        mBreakPoint = bp;
    }

  private:
    bool mIsDebugging = false;
    bool mIsDebuggingRequested = false;
    Gfx* mDlist = nullptr;
    std::vector<const Gfx*> mBreakPoint = {};
};

} // namespace LUS
