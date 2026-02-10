/**
 * @file PriorityThreadPool.cpp
 * @brief Implementation of a Thread Pool with Priority-Based Task Scheduling.
 * * This file demonstrates how to use a std::priority_queue to ensure that 
 * high-importance tasks are executed by worker threads before lower-importance 
 * tasks, regardless of submission order.
 */

#include <iostream>
#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>

/**
 * @struct PriorityTask
 * @brief A wrapper for a callable task associated with a priority level.
 * * The priority level determines the execution order within the @ref PriorityThreadPool.
 */
struct PriorityTask {
    int priority;               ///< Priority level (higher values indicate higher importance).
    std::function<void()> func; ///< The actual task/function to be executed.

    /**
     * @brief Comparator for the priority queue.
     * * Allows @c std::priority_queue to order tasks such that the one with the 
     * highest priority number is at the top.
     * @param other The other task to compare against.
     * @return true if this task has lower priority than the other.
     */
    bool operator<(const PriorityTask& other) const {
        return priority < other.priority; 
    }
};

/**
 * @class PriorityThreadPool
 * @brief A pool of worker threads that retrieves tasks based on priority.
 * * Unlike a standard FIFO (First-In-First-Out) thread pool, this class 
 * uses a heap-based queue to prioritize certain tasks over others.
 */
class PriorityThreadPool {
private:
    std::vector<std::thread> workers;       ///< Persistent worker threads.
    std::priority_queue<PriorityTask> tasks; ///< The internal heap-based task scheduler.
    
    std::mutex queue_mtx;                   ///< Mutex to synchronize access to the priority queue.
    std::condition_variable cv;             ///< Coordinates worker sleep/wake cycles.
    std::atomic<bool> stop;                 ///< Flag to trigger thread pool shutdown.

    /**
     * @brief The worker thread's internal loop.
     * * Workers block until a task is available. When multiple tasks exist, 
     * the one with the highest @ref PriorityTask::priority is selected.
     */
    void workerLoop() {
        while (true) {
            PriorityTask pTask;
            {
                std::unique_lock<std::mutex> lock(this->queue_mtx);
                
                // Wait for a task or a shutdown signal
                while (!this->stop && this->tasks.empty()) {
                    this->cv.wait(lock);
                }

                // Exit condition: stop flag is set and no tasks remain
                if (this->stop && this->tasks.empty()) return;

                // Priority Queue uses .top() (peak at the highest priority element)
                pTask = std::move(const_cast<PriorityTask&>(this->tasks.top()));
                this->tasks.pop();
            }
            if (pTask.func) pTask.func(); 
        }
    }

public:
    /**
     * @brief Initializes the pool and starts worker threads.
     * @param num_threads The number of worker threads to maintain.
     */
    PriorityThreadPool(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back(&PriorityThreadPool::workerLoop, this);
        }
    }

    /**
     * @brief Submits a task with a specific priority level.
     * @param priority Importance level (e.g., 10 for urgent, 1 for background).
     * @param f The function or lambda to execute.
     */
    void enqueue(int priority, std::function<void()> f) {
        {
            std::unique_lock<std::mutex> lock(queue_mtx);
            tasks.push({priority, std::move(f)});
        }
        cv.notify_one();
    }

    /**
     * @brief Gracefully joins all worker threads after processing remaining tasks.
     */
    ~PriorityThreadPool() {
        stop = true;
        cv.notify_all();
        for (auto &w : workers) if (w.joinable()) w.join();
    }
};

/**
 * @struct PaymentTask
 * @brief A Functor simulating a financial processing task.
 */
struct PaymentTask {
    std::string type; ///< Description of the payment activity.
    
    /**
     * @brief Executes the simulated payment task.
     */
    void operator()() const {
        std::cout << "[Worker] Processing: " << type << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
};

/**
 * @brief Main function demonstrating priority-based out-of-order execution.
 */
int main() {
    // We use 1 thread to force tasks to wait in the queue, making 
    // the priority ordering visible in the console output.
    PriorityThreadPool pool(1); 

    std::cout << "Submitting tasks in random order...\n";

    // Submit Low Priority first
    pool.enqueue(1, PaymentTask{"Low: Reward Statement"});
    pool.enqueue(1, PaymentTask{"Low: SMS Notification"});

    // Submit High Priority last
    pool.enqueue(10, PaymentTask{"HIGH: FRAUD DETECTION"});
    pool.enqueue(10, PaymentTask{"HIGH: AUTHORIZATION"});

    // The output will show HIGH priority tasks running before the LOW priority 
    // tasks that were added earlier.
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return 0;
}
