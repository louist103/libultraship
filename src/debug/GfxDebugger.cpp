#include "GfxDebugger.h"
#include <spdlog/fmt/fmt.h>

namespace LUS {

void GfxDebugger::DebugDisplayList(Gfx* cmds) {
    mDlist = cmds;
    mIsDebuggingRequested = false;
    mIsDebugging = true;
    mBreakPoint = { cmds };
}

bool GfxDebugger::HasBreakPoint(const std::vector<const Gfx*>& path) const {
    if (path.size() != mBreakPoint.size())
        return false;

    for (size_t i = 0; i < path.size(); i++) {
        if (path[i] != mBreakPoint[i]) {
            return false;
        }
    }

    return true;
}

} // namespace LUS
