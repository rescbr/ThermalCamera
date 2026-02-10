#ifndef PROFILE_HPP
#define PROFILE_HPP

#include <chrono>
#include <iostream>
#include <string>
#include <cstdlib>

namespace Profile
{
    inline bool IsEnabled() {
        static bool s_checked = false;
        static bool s_enabled = false;
        if (!s_checked) {
            s_enabled = (std::getenv("THERMAL_PROFILE") != nullptr);
            s_checked = true;
        }
        return s_enabled;
    }

    class ScopedTimer
    {
    public:
        ScopedTimer(const char* name)
            : _name(name), _startTime(std::chrono::high_resolution_clock::now())
        {
        }

        ~ScopedTimer()
        {
            if (IsEnabled()) {
                auto endTime = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                    endTime - _startTime).count();
                std::cerr << "[PROFILE] " << _name << ": " << duration << " us ("
                          << (duration / 1000.0) << " ms)" << std::endl;
            }
        }

    private:
        const char* _name;
        std::chrono::high_resolution_clock::time_point _startTime;
    };
}

#define PROFILE_SCOPE(name) Profile::ScopedTimer _timer_##__LINE__(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__func__)

#endif
