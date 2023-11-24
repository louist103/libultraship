#include "GfxDebugger.h"
#include <spdlog/fmt/fmt.h>

namespace LUS {

void GfxDebugger::ResumeGame() {
    mIsDebugging = false;
    mIsDebuggingRequested = false;
    mDlist = nullptr;
}

const Gfx* GfxDebugger::GetDisplayList() const {
    return mDlist;
}

const GfxPath& GfxDebugger::GetBreakPoint() const {
    return mBreakPoint;
}

void GfxDebugger::SetBreakPoint(const GfxPath& bp) {
    mBreakPoint = bp;
}

void GfxDebugger::RequestDebugging() {
    mIsDebuggingRequested = true;
}

bool GfxDebugger::IsDebugging() const {
    return mIsDebugging;
}

bool GfxDebugger::IsDebuggingRequested() const {
    return mIsDebuggingRequested;
}

bool GfxDebugger::HasBreakOnDlist(const std::string& dlistName) {
    return mBreakOnDLs.contains(dlistName);
}

void GfxDebugger::AddBreakOnDlist(const std::string& dlistName) {
    mBreakOnDLs.emplace(dlistName);
}

void GfxDebugger::RemoveBreakOnDlist(const std::string& dlistName) {
    if (mBreakOnDLs.contains(dlistName))
        mBreakOnDLs.erase(dlistName);
}

const std::set<std::string>& GfxDebugger::GetBreakOnDlists() const {
    return mBreakOnDLs;
}

void GfxDebugger::DebugDisplayList(Gfx* cmds) {
    mDlist = cmds;
    mIsDebuggingRequested = false;
    mIsDebugging = true;
    mBreakPoint = {};
    mBreakPoint.push(cmds);
}

bool GfxDebugger::HasBreakPoint(const GfxPath& path) const {
    return path == mBreakPoint;
}

} // namespace LUS
