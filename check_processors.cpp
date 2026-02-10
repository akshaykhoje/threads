#include <iostream>
#include <thread>

int main() {
    unsigned int n_threads = std::thread::hardware_concurrency();
    std::cout << "This CPU has " << n_threads << " threads (logical cores) available." << std::endl;
    return 0;
}
