/**
 * @file BasicThreadPool.cpp
 * @brief A basic implementation of a persistent Worker Thread Pool.
 * * This file demonstrates the core architecture of a thread pool: a fixed 
 * number of threads waiting on a shared task queue using a Monitor Pattern 
 * (Mutex + Condition Variable).
 */

#include <iostream>
#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <chrono>

/**
 * @typedef Task
 * @brief Alias for a callable object with a void() signature.
 */
typedef std::function<void()> Task;

/**
 * @brief Global mutex for synchronized console logging.
 */
std::mutex log_mtx;

/**
 * @class ThreadPool
 * @brief Manages a set of persistent threads to execute fire-and-forget tasks.
 * * This implementation focuses on the "Fire-and-Forget" pattern, where tasks 
 * are submitted for execution without the caller expecting a return value.
 */
class ThreadPool {
private:
    std::vector<std::thread> workers;   ///< The pool of persistent threads.
    std::queue<Task> tasks;             ///< The buffer (queue) for incoming jobs.
    
    std::mutex queue_mtx;               ///< Protects the shared task queue.
    std::condition_variable cv;         ///< Coordinates thread sleep/wake cycles.
    std::atomic<bool> stop;             ///< Flag to trigger a safe shutdown.

    /**
     * @brief The internal loop for each worker thread.
     * * Threads block on the condition variable until @ref enqueue is called 
     * or the pool is destroyed.
     */
    void workerLoop() {
        while (true) {
            Task task; 

            {
                std::unique_lock<std::mutex> lock(this->queue_mtx);
                
                // Wait until there is a task OR a shutdown signal
                while (!this->stop && this->tasks.empty()) {
                    this->cv.wait(lock);
                }

                // Shutdown condition
                if (this->stop && this->tasks.empty()) {
                    return;
                }

                task = std::move(this->tasks.front());
                this->tasks.pop();
            }

            // Execute the task outside the mutex to allow parallelism
            if (task) {
                task(); 
            }
        }
    }

public:
    /**
     * @brief Construct a new Thread Pool and spawns worker threads.
     * @param num_threads The number of worker threads to maintain in the pool.
     */
    ThreadPool(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back(&ThreadPool::workerLoop, this);
        }
    }

    /**
     * @brief Submits a task to the queue for execution.
     * * This is a non-blocking call that moves the task into the internal 
     * queue and notifies a single worker thread.
     * @param t The task to execute (must match void() signature).
     */
    void enqueue(Task t) {
        {
            std::unique_lock<std::mutex> lock(queue_mtx);
            tasks.push(std::move(t));
        }
        cv.notify_one(); 
    }

    /**
     * @brief Destructor that ensures all threads finish current work before exiting.
     * * Signals workers to stop, wakes everyone up, and joins all threads.
     */
    ~ThreadPool() {
        stop = true;
        cv.notify_all();
        
        for (std::thread &worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
};

/**
 * @brief Simulates a CPU-bound data processing task.
 * @param id The task identifier.
 */
void dataProcessingTask(int id) {
    {
        std::lock_guard<std::mutex> lock(log_mtx);
        std::cout << "[Task " << id << "] is being processed by a worker..." << std::endl;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

/**
 * @brief Main function demonstrating mass task submission.
 */
int main() {
    ThreadPool pool(12);

    {
        std::lock_guard<std::mutex> lock(log_mtx);
        std::cout << "--- System Initialized with 12 Worker Threads ---" << std::endl;
    }

    // Submit 1000 tasks
    for (int i = 1; i <= 1000; ++i) {
        pool.enqueue(std::bind(dataProcessingTask, i));
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    return 0;
}
