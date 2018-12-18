// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include "common/common_types.h"
#include "core/hle/applets/erreula.h"
#include "core/hle/applets/swkbd.h"
#include "core/loader/loader.h"
#include "core/memory.h"
#include "core/perf_stats.h"

class Cpu;
class Frontend;

namespace AudioCore {
class DspInterface;
} // namespace AudioCore

namespace Cheats {
class CheatEngine;
} // namespace Cheats

#ifdef ENABLE_SCRIPTING
namespace RPC {
class RPCServer;
} // namespace RPC
#endif

namespace Service::SM {
class ServiceManager;
} // namespace Service::SM

namespace Service::FS {
class ArchiveManager;
} // namespace Service::FS

namespace Kernel {
class KernelSystem;
} // namespace Kernel

namespace Network {
class Room;
class RoomMember;
} // namespace Network

namespace Memory {
class MemorySystem;
} // namespace Memory

namespace Core {

class Movie;
class Timing;

class System {
public:
    ~System();

    void InitNetworkAndMovie();

    /**
     * Gets the instance of the System singleton class.
     * @returns Reference to the instance of the System singleton class.
     */
    static System& GetInstance() {
        return s_instance;
    }

    /// Enumeration representing the return values of the System Initialize and Load process.
    enum class ResultStatus : u32 {
        Success,                        ///< Succeeded
        ErrorNotInitialized,            ///< Error trying to use core prior to initialization
        ErrorGetLoader,                 ///< Error finding the correct program loader
        ErrorSystemMode,                ///< Error determining the system mode
        ErrorLoader,                    ///< Error loading the specified program
        ErrorLoader_ErrorEncrypted,     ///< Error loading the specified program due to encryption
        ErrorLoader_ErrorInvalidFormat, ///< Error loading the specified program due to an
                                        /// invalid format
        ErrorSystemFiles,               ///< Error in finding system files
        ErrorVideoCore,                 ///< Error in the video core
        ErrorVideoCore_ErrorGenericDrivers, ///< Error in the video core due to the user having
                                            /// generic drivers installed
        ErrorVideoCore_ErrorBelowGL33,      ///< Error in the video core due to the user not having
                                            /// OpenGL 3.3 or higher
        ShutdownRequested,                  ///< Emulated program requested a system shutdown
        FatalError
    };

    /**
     * Run the CPU
     * This function runs the core for the specified number of CPU instructions before trying to
     * update hardware. NOTE: the number of instructions requested isn't guaranteed to run, as this
     * will be interrupted preemptively if a hardware update is requested (e.g. on a thread switch)
     * @return Result status, indicating whether or not the operation succeeded.
     */
    ResultStatus Run();

    /// Shutdown the emulated system.
    void Shutdown();

    /// Restart the running program.
    void Restart();

    /// Sets the running program's path. if path is empty, shutdowns the system.
    void SetProgram(const std::string& path);

    /// Closes the running program.
    void CloseProgram();

    /**
     * Load an executable program.
     * @param frontend Reference to the host-system window used for video output and keyboard input.
     * @param filepath String path to the executable program to load on the host file system.
     * @returns ResultStatus code, indicating if the operation succeeded.
     */
    ResultStatus Load(Frontend& frontend, const std::string& filepath);

    /**
     * Indicates if the emulated system is powered on (all subsystems initialized and able to run an
     * program).
     * @returns True if the emulated system is powered on, otherwise false.
     */
    bool IsPoweredOn() const {
        return cpu_core != nullptr;
    }

    /// Prepare the core emulation for a reschedule.
    void PrepareReschedule();

    PerfStats::Results GetAndResetPerfStats();

    /// Gets a reference to the emulated CPU.
    Cpu& CPU() {
        return *cpu_core;
    }

    /// Gets a reference to the emulated DSP.
    AudioCore::DspInterface& DSP() {
        return *dsp_core;
    }

    /// Gets a const reference to the service manager.
    const Service::SM::ServiceManager& ServiceManager() const;

    /// Gets a reference to the service manager.
    Service::SM::ServiceManager& ServiceManager();

    /// Gets a const reference to the archive manager.
    const Service::FS::ArchiveManager& ArchiveManager() const;

    /// Gets a reference to the archive manager.
    Service::FS::ArchiveManager& ArchiveManager();

    /// Gets a const reference to the kernel.
    const Kernel::KernelSystem& Kernel() const;

    /// Gets a reference to the kernel.
    Kernel::KernelSystem& Kernel();

    /// Gets a const reference to the cheat engine.
    const Cheats::CheatEngine& CheatEngine() const;

    /// Gets a reference to the cheat engine.
    Cheats::CheatEngine& CheatEngine();

    /// Gets a const reference to the timing system.
    const Timing& CoreTiming() const;

    /// Gets a reference to the timing system.
    Timing& CoreTiming();

    /// Gets a const reference to the room.
    const Network::Room& Room() const;

    /// Gets a reference to the room.
    Network::Room& Room();

    /// Gets a const reference to the room member.
    const Network::RoomMember& RoomMember() const;

    /// Gets a reference to the room member.
    Network::RoomMember& RoomMember();

    /// Gets a const reference to the movie system.
    const Movie& MovieSystem() const;

    /// Gets a reference to the movie system.
    Movie& MovieSystem();

    /// Gets a const reference to the memory system.
    const Memory::MemorySystem& Memory() const;

    /// Gets a reference to the memory system.
    Memory::MemorySystem& Memory();

    /// Gets a const reference to the frontend.
    const Frontend& GetFrontend() const;

    // Gets a reference to the frontend.
    Frontend& GetFrontend();

    PerfStats perf_stats;
    FrameLimiter frame_limiter;

    void SetStatus(ResultStatus new_status, const char* details = nullptr) {
        status = new_status;
        if (details)
            status_details = details;
    }

    const std::string& GetStatusDetails() const {
        return status_details;
    }

    Loader::ProgramLoader& GetProgramLoader() const {
        return *program_loader;
    }

    bool IsSleepModeEnabled() const {
        return sleep_mode_enabled.load(std::memory_order_relaxed);
    }

    void SetSleepModeEnabled(bool value) {
        sleep_mode_enabled.store(value, std::memory_order_relaxed);
    }

    void SetRunning(bool running) {
        this->running.store(running, std::memory_order::memory_order_relaxed);
        std::unique_lock lock{running_mutex};
        running_cv.notify_one();
    }

    bool IsRunning() {
        return running.load(std::memory_order::memory_order_relaxed);
    }

    std::string GetFilePath() const {
        return m_filepath;
    }

    std::string set_program_file_path;
    std::vector<u8> argument;
    std::vector<u8> hmac;
    u64 argument_source;

private:
    /**
     * Initialize the emulated system.
     * @param frontend Reference to the host-system window used for video output and keyboard input.
     * @param system_mode The system mode.
     * @return ResultStatus code, indicating if the operation succeeded.
     */
    ResultStatus Init(Frontend& frontend, u32 system_mode);

    /// Reschedule the core emulation
    void Reschedule();

    /// ProgramLoader used to load the current executing program
    std::unique_ptr<Loader::ProgramLoader> program_loader;

    /// ARM11 CPU core
    std::unique_ptr<Cpu> cpu_core;

    /// DSP core
    std::unique_ptr<AudioCore::DspInterface> dsp_core;

    /// When true, signals that a reschedule should happen
    bool reschedule_pending{};

    /// Service manager
    std::unique_ptr<Service::SM::ServiceManager> service_manager;

#ifdef ENABLE_SCRIPTING
    /// RPC server for scripting support
    std::unique_ptr<RPC::RPCServer> rpc_server;
#endif

    /// Cheat engine
    std::unique_ptr<Cheats::CheatEngine> cheat_engine;

    /// Archive manager
    std::unique_ptr<Service::FS::ArchiveManager> archive_manager;

    /// Room
    std::unique_ptr<Network::Room> room;

    /// Room member
    std::unique_ptr<Network::RoomMember> room_member;

    // Kernel
    std::unique_ptr<Kernel::KernelSystem> kernel;

    // Timing system
    std::unique_ptr<Timing> timing;

    // Movie system
    std::unique_ptr<Movie> movie;

    // Memory system
    std::unique_ptr<Memory::MemorySystem> memory;

    static System s_instance;

    ResultStatus status;
    std::string status_details;

    Frontend* m_frontend;
    std::string m_filepath;

    std::atomic_bool shutdown_requested;
    std::atomic_bool sleep_mode_enabled;
    std::atomic_bool running;
    std::mutex running_mutex;
    std::condition_variable running_cv;
};

} // namespace Core
