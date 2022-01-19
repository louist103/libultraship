#include "GlobalCtx2.h"
#include <iostream>
#include <vector>
#include "ResourceMgr.h"
#include "Window.h"
#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace Ship {
    std::weak_ptr<GlobalCtx2> GlobalCtx2::Context;

    std::shared_ptr<GlobalCtx2> GlobalCtx2::GetInstance() {
        return Context.lock();
    }

    std::shared_ptr<GlobalCtx2> GlobalCtx2::CreateInstance(std::string Name) {
        if (Context.expired()) {
            auto Shared = std::make_shared<GlobalCtx2>(Name);
            Context = Shared;
            Shared->InitWindow();
            return Shared;
        } else {
            SPDLOG_DEBUG("Trying to create a context when it already exists.");
        }

        return GetInstance();
    }

    GlobalCtx2::GlobalCtx2(std::string Name) : Name(Name), MainPath(""), PatchesPath("") {
        
    }

    GlobalCtx2::~GlobalCtx2() {
        SPDLOG_INFO("destruct GlobalCtx2");
    }

    void GlobalCtx2::InitWindow() {
        InitLogging();
        Config = std::make_shared<ConfigFile>(GlobalCtx2::GetInstance(), "shipofharkinian.ini");
        MainPath = (*Config)["ARCHIVE"]["Main Archive"];
        PatchesPath = (*Config)["ARCHIVE"]["Patches Directory"];
        ResMan = std::make_shared<ResourceMgr>(GlobalCtx2::GetInstance(), MainPath, PatchesPath);
        Win = std::make_shared<Window>(GlobalCtx2::GetInstance());
    }

    void GlobalCtx2::InitLogging() {
        try {
            // Setup Logging
            spdlog::init_thread_pool(8192, 1);
            auto ConsoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            auto FileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/" + GetName() + ".log", 1024 * 1024 * 10, 10);
            ConsoleSink->set_level(spdlog::level::trace);
            FileSink->set_level(spdlog::level::trace);
            std::vector<spdlog::sink_ptr> Sinks{ ConsoleSink, FileSink };
            Logger = std::make_shared<spdlog::async_logger>(GetName(), Sinks.begin(), Sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
            GetLogger()->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%@] [%l] %v");
            spdlog::register_logger(GetLogger());
            spdlog::set_default_logger(GetLogger());
        }
        catch (const spdlog::spdlog_ex& ex) {
            std::cout << "Log initialization failed: " << ex.what() << std::endl;
        }
    }
}