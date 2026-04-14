#pragma once

#include <chrono>
#include <string>
#include <fstream>
#include <mutex>
#include <iostream>
#include <iomanip>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <unistd.h>
#include <fstream>
#endif

namespace wr::core {

    class Profiler {
    public:
        static Profiler& get() {
            static Profiler instance;
            return instance;
        }

        void log(const std::string& systemName, double ms) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_file.is_open()) {
                if (ms > 0.1) {
                    m_file << "[" << systemName << "] " << ms << " ms\n";
                }
            }
        }

        void logSystemStats(int fps, size_t ramMB, size_t entityCount, double cpuPercent) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_file.is_open()) {
                m_file << "[SYS_FPS] " << fps << "\n";
                m_file << "[SYS_RAM] " << ramMB << " MB\n";
                m_file << "[SYS_CPU] " << std::fixed << std::setprecision(2) << cpuPercent << " %\n";
                m_file << "[SYS_ENTITIES] " << entityCount << "\n";
            }
        }

        void logFrameBoundary() {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_file.is_open()) {
                m_file << "--- End of Frame ---\n";
            }
        }

        size_t getProcessMemoryMB() {
        #if defined(_WIN32)
            PROCESS_MEMORY_COUNTERS pmc;
            if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
                return pmc.WorkingSetSize / (1024 * 1024);
            }
            return 0;
        #elif defined(__linux__)
            std::ifstream stat_stream("/proc/self/statm", std::ios_base::in);
            long pages = 0;
            long dummy;
            stat_stream >> dummy >> pages;
            long page_size = sysconf(_SC_PAGE_SIZE);
            return (pages * page_size) / (1024 * 1024);
        #else
            return 0;
        #endif
        }

        double getProcessCPUUsage() {
        #if defined(_WIN32)
            static ULARGE_INTEGER lastTime, lastSysCPU, lastUserCPU;
            static int numProcessors = -1;
            static bool firstRun = true;
            HANDLE self = GetCurrentProcess();

            if (numProcessors == -1) {
                SYSTEM_INFO sysInfo;
                GetSystemInfo(&sysInfo);
                numProcessors = sysInfo.dwNumberOfProcessors;
            }

            FILETIME fnow, fsys, fuser;
            GetSystemTimeAsFileTime(&fnow);
            GetProcessTimes(self, &fnow, &fnow, &fsys, &fuser);

            ULARGE_INTEGER now, sys, user;
            now.LowPart = fnow.dwLowDateTime;
            now.HighPart = fnow.dwHighDateTime;
            sys.LowPart = fsys.dwLowDateTime;
            sys.HighPart = fsys.dwHighDateTime;
            user.LowPart = fuser.dwLowDateTime;
            user.HighPart = fuser.dwHighDateTime;

            if (firstRun) {
                lastTime = now;
                lastSysCPU = sys;
                lastUserCPU = user;
                firstRun = false;
                return 0.0;
            }

            double percent = static_cast<double>((sys.QuadPart - lastSysCPU.QuadPart) + (user.QuadPart - lastUserCPU.QuadPart));
            percent /= static_cast<double>(now.QuadPart - lastTime.QuadPart);
            percent /= numProcessors;

            lastTime = now;
            lastSysCPU = sys;
            lastUserCPU = user;

            return percent * 100.0;
        #else
            return 0.0;
        #endif
        }

    private:
        Profiler() {
            m_file.open("profiler.log", std::ios::out | std::ios::trunc);
            if (!m_file.is_open()) {
                std::cerr << "Failed to open profiler.log\n";
            }
        }
        ~Profiler() {
            if (m_file.is_open()) m_file.close();
        }

        std::ofstream m_file;
        std::mutex m_mutex;
    };

    class ScopedTimer {
    public:
        explicit ScopedTimer(const char* name) : m_name(name) {
            m_start = std::chrono::high_resolution_clock::now();
        }

        ~ScopedTimer() {
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = end - m_start;
            Profiler::get().log(m_name, duration.count());
        }

    private:
        const char* m_name;
        std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
    };

}