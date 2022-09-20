#ifndef T2_UTILITY
#define T2_UTILITY

#include <functional>
#include <chrono>
#include <mutex>

namespace T2 {
    namespace utility {
        // Returns true if the function's timeout kicked in.
        template <typename T>
        bool blocking_timer(const T& duration, bool* const conclusion_flag,
            const std::chrono::milliseconds& intermission = std::chrono::milliseconds(100));
        // Returns true if an exception was caught.
        bool exception_wrapper(const std::function<void()>& fn);

        template <typename T>
        struct mutex_wrapped {
            T object;
            std::mutex mutex;
        };
    };
};

#endif