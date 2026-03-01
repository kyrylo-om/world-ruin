#pragma once

#include <chrono>
#include <string>
#include <fstream>
#include <mutex>
#include <iostream>

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

        void logFrameBoundary() {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_file.is_open()) {
                m_file << "--- End of Frame ---\n";
            }
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