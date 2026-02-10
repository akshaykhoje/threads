#include <iostream>
#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <future>
#include <memory>
#include <utility>

// Type alias for the worker's internal queue
typedef std::function<void()> Task;

// Global log mutex to keep our CLI output clean
std::mutex log_mtx;

// --- 1. The Wrapper Functor ---
// This replaces the lambda we would normally use to wrap the packaged_task.
// It allows the worker to call (*task)() without knowing the return type.
struct TaskWrapper {
    std::shared_ptr<std::packaged_task<void()>> ptr;
    void operator()() {
        if (ptr) (*ptr)();
    }
};

class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<Task> tasks;
    std::mutex queue_mtx;
    std::condition_variable cv;
    std::atomic<bool> stop;

    // Member function for the persistent worker threads
    void workerLoop() {
        while (true) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(this->queue_mtx);
                while (!this->stop && this->tasks.empty()) {
                    this->cv.wait(lock);
                }
                if (this->stop && this->tasks.empty()) return;

                task = std::move(this->tasks.front());
                this->tasks.pop();
            }
            if (task) task(); 
        }
    }

public:
    ThreadPool(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back(&ThreadPool::workerLoop, this);
        }
    }

    // --- 2. The Request-Response Enqueue ---
    // Uses 'auto' and 'decltype' to deduce the return type of the task.
    template<class F>
    auto enqueue(F f) -> std::future<decltype(f())> {
        using ReturnType = decltype(f());

        // Create the packaged task on the heap so it can be shared
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(f);
        
        // Get the future "ticket" to return to the caller
        std::future<ReturnType> res = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mtx);
            // We wrap the packaged_task in a simple void() functor.
            // When the worker calls this, it triggers the packaged_task.
            tasks.emplace([task]() { (*task)(); });
        }

        cv.notify_one();
        return res;
    }

    ~ThreadPool() {
        stop = true;
        cv.notify_all();
        for (std::thread &worker : workers) {
            if (worker.joinable()) worker.join();
        }
    }
};

// --- 3. Example Functions (Tasks) ---

int multiply(int a, int b) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return a * b;
}

std::string fetchMetadata(int id) {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    return "Metadata_for_ID_" + std::to_string(id);
}

std::pair<bool, int> isPrime(int n) {
    if (n <= 1) return {false, n};
    for (int i = 2; i * i <= n; i++) {
        if (n % i == 0) return {false, n};
    }
    return {true, n};
}

std::string fetchUserCity(int userId) {
    std::this_thread::sleep_for(std::chrono::milliseconds(800)); // Simulate latency
    if(userId == 1) return "Pune";
    return "Unknown";
}

double getDiskUsage(std::string path) {
    // Simulated system call
    return 75.5;
}

std::string encryptData(std::string raw) {
    std::reverse(raw.begin(), raw.end()); // Simple "encryption" for demo
    return "ENC_" + raw;
}

double calculateInterest(double principal, double rate, int years) {
    return principal * rate * years / 100.0;
}

// --- 4. CLI Test Harness ---

int main() {
    int persistent_threads = std::thread::hardware_concurrency();
    ThreadPool pool(persistent_threads);
    std::cout << "Using " << persistent_threads << " for  the program...";

    // Submission Phase: We get 'Futures' back immediately
    std::future<int> result1 = pool.enqueue(std::bind(multiply, 10, 5));
    std::future<std::string> result2 = pool.enqueue(std::bind(fetchMetadata, 101));
    // auto f1 = pool.enqueue(std::bind(isPrime, 11));     // returns bool
    auto f2 = pool.enqueue(std::bind(fetchUserCity, 1));    // returns string
    auto f3 = pool.enqueue(std::bind(getDiskUsage, "/var/log"));    // returns double
    auto f4 = pool.enqueue(std::bind(encryptData, "SomeRandomString"));     // returns string
    auto f5 = pool.enqueue(std::bind(calculateInterest, 5000, 7, 5));   // returns double
    
    std::vector<std::future<std::pair<bool, int>>> results;
    for(int i=2; i<=500; i++) {
        results.push_back(pool.enqueue(std::bind(isPrime, i)));
    }

    {
        std::lock_guard<std::mutex> lock(log_mtx);
        std::cout << "[Main] All NEW tasks submitted to the pool." << std::endl;
    }
    
    // Response Phase: We 'get()' the results (this blocks if not ready)
    // This is the "Response" part of the Request-Response pattern.
    std::cout << "Multiplication Result: " << result1.get() << std::endl;
    std::cout << "Fetch Result: " << result2.get() << std::endl;
    // std::cout << "1. Is Prime: " << (f1.get() ? "Yes" : "No") << std::endl;
    std::cout << "2. User City: " << f2.get() << std::endl;
    for(auto &f : results) {
        std::pair<bool, int> result = f.get();
        if(result.first) {
            std::lock_guard<std::mutex> log(log_mtx);
            std::cout << result.second << " is prime!" << std::endl;
        }
    }
    std::cout << "3. Disk Usage: " << f3.get() << "%" << std::endl;
    std::cout << "4. Encrypted: " << f4.get() << std::endl;
    std::cout << "5. Interest: " << f5.get() << " INR" << std::endl;

    return 0;
}