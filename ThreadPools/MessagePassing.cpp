/**
 * @file MessagePassingThreadPool.cpp
 * @brief Thread Pool implementation using a centralized Result Queue (Inbox pattern).
 * * This file demonstrates a decoupled architecture where worker threads push results
 * to a thread-safe "Inbox," and the main thread consumes them asynchronously.
 */

#include <iostream>
#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <utility>

/**
 * @class ResultQueue
 * @brief A thread-safe template queue used for inter-thread communication.
 * * This acts as the "Inbox." Producers (workers) push results into it, 
 * and the Consumer (main thread) pops them out.
 * @tparam T The type of data being passed between threads.
 */
template <typename T>
class ResultQueue {
private:
    std::queue<T> queue;            ///< Internal storage for the results.
    std::mutex mtx;                 ///< Mutex to protect the internal queue.
    std::condition_variable cv;     ///< Condition variable to signal the main thread.

public:
    /**
     * @brief Pushes a result into the queue and notifies the waiting thread.
     * @param result The data object to be sent to the inbox.
     */
    void push(T result) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push(result);
        }
        cv.notify_one(); 
    }

    /**
     * @brief Blocks until a result is available and then removes it from the queue.
     * @return T The retrieved result object.
     */
    T pop() {
        std::unique_lock<std::mutex> lock(mtx);
        // Wait until the inbox is not empty
        cv.wait(lock, [this] { return !this->queue.empty(); });
        T val = std::move(this->queue.front());
        queue.pop();
        return val;
    }
};

/**
 * @typedef Task
 * @brief Alias for a generic callable object.
 */
typedef std::function<void()> Task;

/**
 * @class ThreadPool
 * @brief A pool of persistent workers that execute generic Task objects.
 * * This pool is oblivious to the return types of tasks; it simply executes them.
 * Communication of results is handled externally by the tasks themselves.
 */
class ThreadPool {
private:
    std::vector<std::thread> workers;   ///< Collection of persistent threads.
    std::queue<Task> tasks;             ///< Queue of incoming work units.
    std::mutex queue_mtx;               ///< Mutex for task queue safety.
    std::condition_variable cv;         ///< Signals workers when work is available.
    std::atomic<bool> stop;             ///< Shutdown flag.

    /**
     * @brief Persistent loop for worker threads to fetch and execute tasks.
     */
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
    /**
     * @brief Constructs the pool and starts the worker threads.
     * @param num_threads Number of threads to spawn.
     */
    ThreadPool(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back(&ThreadPool::workerLoop, this);
        }
    }

    /**
     * @brief Enqueues a Task (functor/lambda) into the work queue.
     * @param t The task to be executed by a worker.
     */
    void enqueue(Task t) {
        {
            std::unique_lock<std::mutex> lock(queue_mtx);
            tasks.push(std::move(t));
        }
        cv.notify_one();
    }

    /**
     * @brief Joins all threads for a clean shutdown.
     */
    ~ThreadPool() {
        stop = true;
        cv.notify_all();
        for (std::thread &worker : workers) {
            if (worker.joinable()) worker.join();
        }
    }
};

/**
 * @struct IsPrimeCalculator
 * @brief A Functor (Function Object) that calculates primality.
 * * Instead of returning a value, it holds a pointer to a @ref ResultQueue 
 * and "pushes" its result there once finished.
 */
struct IsPrimeCalculator {
    int n;                                          ///< The number to check for primality.
    ResultQueue<std::pair<bool, int>>* outputQueue;  ///< Pointer to the shared inbox.

    /**
     * @brief Overloaded operator() to perform the calculation.
     * * This allows the struct instance to be treated as a void() task by the @ref ThreadPool.
     */
    void operator()() const {
        bool prime = true;
        if (n <= 1) prime = false;
        else {
            for (int i = 2; i * i <= n; i++) {
                if (n % i == 0) {
                    prime = false;
                    break;
                }
            }
        }
        // Send the result to the orchestrator (Main Thread)
        outputQueue->push({prime, n});
    }
};

/**
 * @brief Orchestrates the mass calculation and collection of results.
 * * Uses the hardware concurrency count to optimize the thread pool size.
 */
int main() {
    int core_count = std::thread::hardware_concurrency();
    ThreadPool pool(core_count);
    ResultQueue<std::pair<bool, int>> results;

    std::cout << "System started with " << core_count << " workers.\n";

    // Submission: Asynchronous and non-blocking
    for (int i = 1; i <= 500; ++i) {
        IsPrimeCalculator taskObj;
        taskObj.n = i;
        taskObj.outputQueue = &results;
        pool.enqueue(taskObj);
    }

    // Collection: The "Inbox" processing loop
    for (int i = 1; i <= 500; ++i) {
        std::pair<bool, int> res = results.pop();
        if (res.first) {
            std::cout << "[Main] Received Result: " << res.second << " is PRIME\n";
        }
    }

    std::cout << "All tasks processed asynchronously!\n";
    return 0;
}
