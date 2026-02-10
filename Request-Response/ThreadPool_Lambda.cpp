/**
 * @file ThreadPool_Lambda.cpp
 * @brief A high-performance, template-based Thread Pool with Future support.
 * * This file implements a pool of worker threads that can execute arbitrary 
 * functions and return results asynchronously using C++11/14 features.
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

/**
 * @typedef Task
 * @brief Type alias for a basic void function with no arguments.
 */
typedef std::function<void()> Task;

/**
 * @class ThreadPool
 * @brief Manages a collection of threads that execute tasks from a shared queue.
 * * The pool provides a mechanism to offload work to background threads and 
 * retrieve return values via std::future, preventing the main thread from blocking.
 */
class ThreadPool_Lambda {
private:
    std::vector<std::thread> workers;   ///< List of worker threads.
    std::queue<Task> tasks;             ///< Internal FIFO queue of pending tasks.
    std::mutex queue_mtx;               ///< Mutex to synchronize access to the task queue.
    std::condition_variable cv;         ///< Condition variable to notify workers of new tasks.
    std::atomic<bool> stop;             ///< Atomic flag to signal pool shutdown.

    /**
     * @brief The core loop executed by every worker thread.
     * * Workers wait on the condition variable until a task is available or the 
     * pool is stopped. They process tasks until the queue is empty and @ref stop is true.
     */
    void workerLoop() {
        while (true) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(this->queue_mtx);
                // Wait until there is a task OR we are stopping
                while (!this->stop && this->tasks.empty()) {
                    this->cv.wait(lock);
                }
                
                // Exit condition: stop flag is set and no tasks remain
                if (this->stop && this->tasks.empty()) return;

                task = std::move(this->tasks.front());
                this->tasks.pop();
            }
            if (task) task(); // Execute the task
        }
    }

public:
    /**
     * @brief Construct a new Thread Pool object.
     * @param num_threads The number of worker threads to spawn.
     */
    ThreadPool_Lambda(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back(&ThreadPool_Lambda::workerLoop, this);
        }
    }

    /**
     * @brief Enqueues a function for asynchronous execution.
     * * This template method wraps a function into a packaged_task, pushes it 
     * to the queue, and returns a future to track the result.
     * * @tparam F The type of the function to be executed.
     * @param f The function (or lambda/bind) to execute.
     * @return std::future<decltype(f())> A future object to retrieve the function's result.
     */
    template<class F>
    auto enqueue(F f) -> std::future<decltype(f())> {
        // 1. Determine the return type of the function dynamically
        using ReturnType = decltype(f());

        // 2. Wrap the function in a packaged_task shared pointer
        // Packaged tasks can only be moved, so we use shared_ptr to allow copying into the wrapper lambda.
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(f);

        // 3. Extract the future "ticket" before sending the task to the queue
        std::future<ReturnType> res = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mtx);
            
            // 4. Wrap the task in a void() lambda so it fits our Task queue
            tasks.emplace([task]() { (*task)(); });
        }

        cv.notify_one(); // Wake up one idle worker
        return res;      // Return the future to the caller
    }

    /**
     * @brief Destroy the Thread Pool object.
     * * Signals all workers to stop and joins them. Tasks already in the 
     * queue will be completed before destruction finishes.
     */
    ~ThreadPool_Lambda() {
        stop = true;
        cv.notify_all(); // Wake up all threads to see the stop flag
        for (std::thread &worker : workers) {
            if (worker.joinable()) worker.join();
        }
    }
};

/**
 * @brief Simulates a file upload operation.
 * @param name Name of the file to upload.
 * @return true on successful "upload".
 */
bool uploadFile(std::string name) {
    std::cout << "Uploading " << name << "..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return true; 
}

/**
 * @brief Main function demonstrating ThreadPool_Lambda usage with futures.
 */
int main() {
    ThreadPool_Lambda pool(4);

    // Enqueue tasks and store the futures
    std::future<bool> f1 = pool.enqueue(std::bind(uploadFile, "Agreement.pdf"));
    std::future<bool> f2 = pool.enqueue(std::bind(uploadFile, "Receipt.pdf"));

    std::cout << "Doing other work..." << std::endl;

    // Retrieve results; these calls block until the corresponding tasks finish
    if (f1.get() && f2.get()) {
        std::cout << "All files uploaded successfully!" << std::endl;
    }

    return 0;
}
