#include <iostream>
#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm> // for make_heap, push_heap, pop_heap
#include <chrono>

using namespace std::chrono;

// --- 1. THE AGED TASK DATA ---
struct AgedTask {
    int original_priority;
    int current_priority;
    system_clock::time_point arrival_time;
    std::function<void()> func;
    std::string task_name;

    // The Heap needs to know how to sort: Higher priority moves to the front
    bool operator<(const AgedTask& other) const {
        return current_priority < other.current_priority;
    }
};

class AgingPriorityPool {
private:
    std::vector<std::thread> workers;
    std::thread monitor_thread; // NEW: Dedicated thread for aging
    std::vector<AgedTask> task_heap;
    std::mutex queue_mtx;
    std::condition_variable cv;
    std::atomic<bool> stop;

    void monitorLoop() {
        while (!stop) {
            std::this_thread::sleep_for(milliseconds(1000)); // Check every second
            {
                std::lock_guard<std::mutex> lock(queue_mtx);
                applyAging();
            }
            cv.notify_all(); // Notify workers that priorities might have changed
        }
    }

    void applyAging() {
        auto now = system_clock::now();
        bool needs_reheap = false;
        for (auto& task : task_heap) {
            auto wait_duration = duration_cast<seconds>(now - task.arrival_time).count();
            int age_bonus = (static_cast<int>(wait_duration) / 2) * 20; // Faster aging for demo
            
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

    void workerLoop() {
        while (true) {
            AgedTask activeTask;
            {
                std::unique_lock<std::mutex> lock(this->queue_mtx);
                while (!this->stop && task_heap.empty()) {
                    cv.wait(lock);
                }
                if (this->stop && task_heap.empty()) return;

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
    AgingPriorityPool(size_t threads) : stop(false) {
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back(&AgingPriorityPool::workerLoop, this);
        }
        // Start the background aging monitor
        monitor_thread = std::thread(&AgingPriorityPool::monitorLoop, this);
    }

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
            std::push_heap(task_heap.begin(), task_heap.end()); // Maintain heap property
        }
        cv.notify_one();
    }

    ~AgingPriorityPool() {
        stop = true;
        cv.notify_all();
        if (monitor_thread.joinable()) monitor_thread.join();
        for (auto &w : workers) if (w.joinable()) w.join();
    }
};

// --- 2. EXAMPLE WORK FUNCTOR ---
struct HeavyTask {
    int duration_ms;
    void operator()() const {
        std::this_thread::sleep_for(milliseconds(duration_ms));
    }
};

// --- 3. TEST HARNESS ---
int main() {
    // 1. Create a pool with ONLY 1 thread to ensure tasks must wait
    AgingPriorityPool pool(1);

    std::cout << "--- STARTING AGING DEMONSTRATION ---\n";
    std::cout << "Step 1: Submitting a 'Long' High-Priority task to block the worker...\n";
    pool.enqueue(100, "BLOCKING_TASK", HeavyTask{4000}); // Takes 4 seconds

    std::cout << "Step 2: Submitting a Low-Priority task (Priority 10) that will be 'starved'...\n";
    pool.enqueue(20, "STARVED_REWARD_TASK", HeavyTask{500});

    std::cout << "Step 3: Flooding the queue with Medium-Priority tasks (Priority 50)...\n";
    // Normally, these would ALL run before the STARVED_REWARD_TASK.
    for(int i = 1; i <= 20; ++i) {
        pool.enqueue(30, "MEDIUM_TASK_" + std::to_string(i), HeavyTask{1000});
    }

    std::cout << "\n--- OBSERVATION PERIOD ---\n";
    std::cout << "Watch as STARVED_REWARD_TASK's priority climbs every 3 seconds.\n";
    std::cout << "Eventually, it will exceed 50 and jump ahead of the MEDIUM_TASKS!\n\n";

    // Keep main alive long enough to see the full lifecycle
    std::this_thread::sleep_for(std::chrono::seconds(20));

    return 0;
}
