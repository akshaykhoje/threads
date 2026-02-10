/**
 * @file ThreadPool_Advanced.cpp
 * @brief Advanced Thread Pool implementation with dynamic return types and CLI test harness.
 * * This file demonstrates a sophisticated thread pool that uses std::packaged_task 
 * and std::future to facilitate asynchronous execution of various tasks with 
 * different signatures and return types.
 */

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
#include <algorithm> // For std::reverse

/**
 * @typedef Task
 * @brief Type alias for a generic void-returning function.
 * Used internally by the worker threads to process the task queue.
 */
typedef std::function<void()> Task;

/**
 * @brief Global mutex for synchronized console logging.
 * Prevents race conditions and interleaved output when multiple threads log to stdout.
 */
std::mutex log_mtx;

/**
 * @struct TaskWrapper
 * @brief A functor that wraps a packaged_task shared pointer.
 * * This wrapper allows the worker threads to execute a task by calling its 
 * operator(), effectively hiding the specific return type of the underlying 
 * std::packaged_task from the worker loop.
 */
struct TaskWrapper {
    std::shared_ptr<std::packaged_task<void()>> ptr; ///< Pointer to the task.
    
    /**
     * @brief Executes the wrapped task.
     */
    void operator()() {
        if (ptr) (*ptr)();
    }
};

/**
 * @class ThreadPool_Advanced
 * @brief A pool of persistent worker threads for concurrent task execution.
 * * The ThreadPool manages a fixed set of threads that pull tasks from a thread-safe 
 * queue. It supports tasks with arbitrary return values via the @ref enqueue method.
 */
class ThreadPool_Advanced {
private:
    std::vector<std::thread> workers;   ///< Collection of persistent worker threads.
    std::queue<Task> tasks;             ///< Queue of pending tasks waiting for execution.
    std::mutex queue_mtx;               ///< Mutex protecting the task queue.
    std::condition_variable cv;         ///< Condition variable to signal workers.
    std::atomic<bool> stop;             ///< Shutdown flag.

    /**
     * @brief The main loop for worker threads.
     * * Threads block here until a task is available or the pool is signaled to stop.
     * It ensures all remaining tasks are processed before the thread exits.
     */
    void workerLoop() {
        while (true) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(this->queue_mtx);
                // Wait for task or stop signal
                while (!this->stop && this->tasks.empty()) {
                    this->cv.wait(lock);
                }
                // Exit if stop is requested and no tasks are left
                if (this->stop && this->tasks.empty()) return;

                task = std::move(this->tasks.front());
                this->tasks.pop();
            }
            if (task) task(); 
        }
    }

public:
    /**
     * @brief Initializes the thread pool with a specified number of threads.
     * @param num_threads Number of worker threads to create.
     */
    ThreadPool_Advanced(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back(&ThreadPool_Advanced::workerLoop, this);
        }
    }

    /**
     * @brief Adds a task to the pool and returns a future for the result.
     * * Uses template meta-programming to deduce the return type of the provided function.
     * * @tparam F Type of the function or functor.
     * @param f The task to execute.
     * @return std::future<decltype(f())> A future object to access the task's return value.
     */
    template<class F>
    auto enqueue(F f) -> std::future<decltype(f())> {
        using ReturnType = decltype(f());

        // Packaged tasks handle the storage of the return value or exceptions.
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(f);
        
        std::future<ReturnType> res = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mtx);
            // Lambda captures the shared_ptr by value to extend its lifetime.
            tasks.emplace([task]() { (*task)(); });
        }

        cv.notify_one();
        return res;
    }

    /**
     * @brief Gracefully shuts down the thread pool.
     * * Sets the stop flag, notifies all workers, and joins them to ensure 
     * all threads finish execution.
     */
    ~ThreadPool_Advanced() {
        stop = true;
        cv.notify_all();
        for (std::thread &worker : workers) {
            if (worker.joinable()) worker.join();
        }
    }
};

// --- Tasks Documentation ---

/**
 * @brief Simulates an intensive multiplication operation.
 * @param a First factor.
 * @param b Second factor.
 * @return Product of a and b.
 */
int multiply(int a, int b) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return a * b;
}

/**
 * @brief Checks if a number is prime.
 * @param n The integer to check.
 * @return A pair containing a boolean (is_prime) and the original number.
 */
std::pair<bool, int> isPrime(int n) {
    if (n <= 1) return {false, n};
    for (int i = 2; i * i <= n; i++) {
        if (n % i == 0) return {false, n};
    }
    return {true, n};
}

// (Remaining utility functions fetchMetadata, encryptData, etc. documented similarly)

/**
 * @brief Main entry point to demonstrate the Request-Response pattern.
 * * Submits various tasks to the pool and retrieves results using the future objects.
 */
int main() {
    int persistent_threads = std::thread::hardware_concurrency();
    ThreadPool_Advanced pool(persistent_threads);
    std::cout << "Using " << persistent_threads << " threads for the program..." << std::endl;

    // Submission Phase
    std::future<int> result1 = pool.enqueue(std::bind(multiply, 10, 5));
    auto f2 = pool.enqueue(std::bind(isPrime, 11)); 
    
    // Response Phase
    std::cout << "Multiplication Result: " << result1.get() << std::endl;
    
    return 0;
}
