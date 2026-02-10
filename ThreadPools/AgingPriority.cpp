/**
 * @file AgingPriorityPool.cpp
 * @brief Implementation of a Thread Pool with dynamic Task Aging to prevent Starvation.
 * * This file demonstrates a sophisticated scheduling system where a background monitor
 * thread periodically increases the priority of waiting tasks based on their 
 * residency time in the heap.
 */

#include <iostream>
#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm>
#include <chrono>

using namespace std::chrono;

/**
 * @struct AgedTask
 * @brief Represents a task whose priority can increase over time.
 * * Contains metadata regarding original priority, current (aged) priority, 
 * and arrival time to facilitate the aging calculation.
 */
struct AgedTask {
    int original_priority;              ///< The priority assigned at submission.
    int current_priority;               ///< The boosted priority after aging.
    system_clock::time_point arrival_time; ///< Timestamp of when the task entered the pool.
    std::function<void()> func;         ///< The callable task logic.
    std::string task_name;              ///< Human-readable name for logging.

    /**
     * @brief Heap comparator. 
     * Higher @ref current_priority values are moved to the front of the heap.
     */
    bool operator<(const AgedTask& other) const {
        return current_priority < other.current_priority;
    }
};

/**
 * @class AgingPriorityPool
 * @brief A priority-based thread pool that implements an Aging Algorithm.
 * * This pool uses a `std::vector` as a binary heap and a dedicated monitor thread
 * to "age" tasks, ensuring that low-priority tasks eventually gain enough 
 * priority to be executed even under high load.
 */
class AgingPriorityPool {
private:
    std::vector<std::thread> workers;   ///< Worker threads processing the heap.
    std::thread monitor_thread;         ///< Background thread responsible for aging logic.
    std::vector<AgedTask> task_heap;    ///< Internal storage managed as a Max-Heap.
    std::mutex queue_mtx;               ///< Protects heap integrity during aging and execution.
    std::condition_variable cv;         ///< Signals workers when tasks are added or aged.
    std::atomic<bool> stop;             ///< Shutdown flag.

    /**
     * @brief Background loop that triggers priority aging every second.
     */
    void monitorLoop() {
        while (!stop) {
            std::this_thread::sleep_for(milliseconds(1000));
            {
                std::lock_guard<std::mutex> lock(queue_mtx);
                applyAging();
            }
            cv.notify_all(); // Re-wake workers as heap order may have shifted
        }
    }

    /**
     * @brief Recalculates priorities for all waiting tasks.
     * * If a task has waited for a multiple of 2 seconds, its priority is 
     * significantly boosted. If any priorities change, the heap is rebuilt.
     */
    void applyAging() {
        auto now = system_clock::now();
        bool needs_reheap = false;
        for (auto& task : task_heap) {
            auto wait_duration = duration_cast<seconds>(now - task.arrival_time).count();
            
            // Boost priority by 20 for every 2 seconds of waiting
            int age_bonus = (static_cast<int>(wait_duration) / 2) * 20; 
            
            if (age_bonus > 0) {
                int new_prio = task.original_priority + age_bonus;
                if(new_prio > task.current_priority) {
                    task.current_priority = new_prio;
                    needs_reheap = true;
                }
            }
        }
        if (needs_reheap) {
            std::make_heap(task_heap.begin(), task_heap.end());
        }
    }

    /**
     * @brief Core worker loop that pops the highest priority task from the heap.
     */
    void workerLoop() {
        while (true) {
            AgedTask activeTask;
            {
                std::unique_lock<std::mutex> lock(this->queue_mtx);
                while (!this->stop && task_heap.empty()) {
                    cv.wait(lock);
                }
                if (this->stop && task_heap.empty()) return;

                // Move top of heap to the back, then pop
                std::pop_heap(task_heap.begin(), task_heap.end());
                activeTask = std::move(task_heap.back());
                task_heap.pop_back();
            }
            if (activeTask.func) {
                std::cout << "[Worker] Starting: " << activeTask.task_name 
                          << " | Priority: " << activeTask.current_priority << std::endl;
                activeTask.func();
            }
        }
    }

public:
    /**
     * @brief Constructs the pool and starts both workers and the aging monitor.
     * @param threads Number of worker threads to spawn.
     */
    AgingPriorityPool(size_t threads) : stop(false) {
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back(&AgingPriorityPool::workerLoop, this);
        }
        monitor_thread = std::thread(&AgingPriorityPool::monitorLoop, this);
    }

    /**
     * @brief Enqueues a task with an initial priority.
     * @param priority Initial importance level.
     * @param name Name for identifying the task in logs.
     * @param f The function to execute.
     */
    void enqueue(int priority, std::string name, std::function<void()> f) {
        {
            std::lock_guard<std::mutex> lock(queue_mtx);
            AgedTask newTask;
            newTask.original_priority = priority;
            newTask.current_priority = priority;
            newTask.arrival_time = system_clock::now();
            newTask.func = std::move(f);
            newTask.task_name = name;

            task_heap.push_back(std::move(newTask));
            std::push_heap(task_heap.begin(), task_heap.end());
        }
        cv.notify_one();
    }

    /**
     * @brief Gracefully shuts down the monitor and worker threads.
     */
    ~AgingPriorityPool() {
        stop = true;
        cv.notify_all();
        if (monitor_thread.joinable()) monitor_thread.join();
        for (auto &w : workers) if (w.joinable()) w.join();
    }
};

/**
 * @struct HeavyTask
 * @brief A simulated CPU-intensive workload functor.
 * * This functor is used to occupy worker threads for a specific duration, 
 * allowing the priority queue to fill up and the aging monitor to trigger.
 */
struct HeavyTask {
    int duration_ms; ///< Time in milliseconds the task will "occupy" the worker.

    /**
     * @brief Executes the simulated work by sleeping the worker thread.
     */
    void operator()() const {
        std::this_thread::sleep_for(milliseconds(duration_ms));
    }
};

/**
 * @brief The Main Test Harness for the Aging Priority Pool.
 * * This demonstration performs the following steps:
 * 1. Spawns a pool with a single thread to force queueing.
 * 2. Submits a long-running "Blocking Task" to hold the worker.
 * 3. Submits a "Starved" low-priority task.
 * 4. Floods the queue with "Medium" priority tasks.
 * 5. Observes the aging monitor as it boosts the starved task ahead of the flood.
 * * @return int Execution status code.
 */
int main() {
    // We use ONLY 1 thread so that every task after the first one 
    // MUST wait in the heap, allowing us to observe the aging process.
    AgingPriorityPool pool(1);

    std::cout << "--- STARTING AGING DEMONSTRATION ---\n";
    
    // Step 1: Block the worker for 4 seconds
    std::cout << "Step 1: Submitting 'BLOCKING_TASK' (Prio: 100)...\n";
    pool.enqueue(100, "BLOCKING_TASK", HeavyTask{4000}); 

    // Step 2: Add a task that would normally wait forever (Starvation)
    std::cout << "Step 2: Submitting 'STARVED_REWARD_TASK' (Prio: 20)...\n";
    pool.enqueue(20, "STARVED_REWARD_TASK", HeavyTask{500});

    // Step 3: Flood the system with medium tasks (Prio: 50)
    std::cout << "Step 3: Flooding queue with 20 'MEDIUM_TASKS' (Prio: 50)...\n";
    for(int i = 1; i <= 20; ++i) {
        pool.enqueue(50, "MEDIUM_TASK_" + std::to_string(i), HeavyTask{1000});
    }

    std::cout << "\n--- OBSERVATION PERIOD ---\n";
    std::cout << "The 'STARVED' task starts at Priority 20.\n";
    std::cout << "Every 2 seconds, it gains +20 priority.\n";
    std::cout << "Wait ~4 seconds: It becomes Priority 60 and jumps ahead of all Medium Tasks!\n\n";

    // Keep the main thread alive to watch the console logs as workers process the aged tasks
    std::this_thread::sleep_for(std::chrono::seconds(20));

    return 0;
}
