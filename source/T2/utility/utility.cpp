#include "./utility.hpp"

#include <thread>
#include <iostream>

template <typename T>
bool T2::utility::blocking_timer(const T& duration,
    bool* const conclusion_flag, const std::chrono::milliseconds& intermission) {

    const std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<T>(std::chrono::steady_clock::now() - start_time) < duration &&
        !*conclusion_flag) {
        std::this_thread::sleep_for(intermission);
    }
    return !*conclusion_flag;
}

// TIL: https://stackoverflow.com/q/115703
template bool T2::utility::blocking_timer<std::chrono::milliseconds>(
    const std::chrono::milliseconds& duration, bool* const conclusion_flag,
    const std::chrono::milliseconds& intermission);

bool T2::utility::exception_wrapper(const std::function<void()>& fn) {
    try {
        fn();
    }
    catch (...) {
#if defined(_DEBUG)
        std::cerr << "T2::utility::exception_wrapper() @ " + std::to_string(__LINE__) + "\r\n";
#endif
        return true;
    }
    return false;
}