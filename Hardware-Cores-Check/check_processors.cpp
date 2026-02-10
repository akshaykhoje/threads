/**
 * @file check_processors.cpp
 * @brief Utility to determine the optimal number of threads for the host system.
 * * This program queries the hardware to find the number of supported concurrent threads.
 * This value is critical for avoiding over-subscription in Thread Pools.
 */

#include <iostream>
#include <thread>

/**
 * @brief Main entry point.
 * @return 0 on success.
 */
int main() {
    /**
     * @brief The number of concurrent threads supported by the implementation.
     * * The value should be considered a hint. If the value is not computable or well defined, 
     * this function returns 0.
     */
    unsigned int n_threads = std::thread::hardware_concurrency();

    std::cout << "--- Hardware Concurrency Check ---" << std::endl;
    if (n_threads == 0) {
        std::cout << "Hardware concurrency not detectable. Defaulting to fallback." << std::endl;
    } else {
        std::cout << "Detected " << n_threads << " concurrent threads supported by hardware." << std::endl;
    }

    return 0;
}
