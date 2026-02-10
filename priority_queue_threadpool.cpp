#include <iostream>
#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>

// --- 1. THE PRIORITY TASK STRUCTURE ---
struct PriorityTask {
    int priority;
    std::function<void()> func;

    // We need to define "less than" so the priority_queue knows how to sort.
    // Higher priority number = Higher importance.
    bool operator<(const PriorityTask& other) const {
        return priority < other.priority; 
    }
};

class PriorityThreadPool {
private:
    std::vector<std::thread> workers;
    // We swap std::queue for std::priority_queue
    std::priority_queue<PriorityTask> tasks; 
    
    std::mutex queue_mtx;
    std::condition_variable cv;
    std::atomic<bool> stop;

    void workerLoop() {
        while (true) {
            PriorityTask pTask;
            {
                std::unique_lock<std::mutex> lock(this->queue_mtx);
                while (!this->stop && this->tasks.empty()) {
                    this->cv.wait(lock);
                }
                if (this->stop && this->tasks.empty()) return;

                // Priority Queue uses .top() instead of .front()
                pTask = std::move(this->tasks.top());
                this->tasks.pop();
            }
            if (pTask.func) pTask.func(); 
        }
    }

public:
    PriorityThreadPool(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back(&PriorityThreadPool::workerLoop, this);
        }
    }

    // Enqueue now requires a priority level
    void enqueue(int priority, std::function<void()> f) {
        {
            std::unique_lock<std::mutex> lock(queue_mtx);
            tasks.push({priority, std::move(f)});
        }
        cv.notify_one();
    }

    ~PriorityThreadPool() {
        stop = true;
        cv.notify_all();
        for (auto &w : workers) if (w.joinable()) w.join();
    }
};

// --- 3. THE LOGIC (Functor Style) ---
struct PaymentTask {
    std::string type;
    void operator()() const {
        std::cout << "[Worker] Processing: " << type << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
};

int main() {
    PriorityThreadPool pool(1); // Using 1 thread to clearly see the priority order

    std::cout << "Submitting tasks in random order...\n";

    // We submit Low Priority first
    pool.enqueue(1, PaymentTask{"Low: Reward Statement"});
    pool.enqueue(1, PaymentTask{"Low: SMS Notification"});

    // We submit High Priority last
    pool.enqueue(10, PaymentTask{"HIGH: FRAUD DETECTION"});
    pool.enqueue(10, PaymentTask{"HIGH: AUTHORIZATION"});

    // Even though the Low priority tasks were added first, 
    // the worker will pick the High priority ones as soon as it's free.
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return 0;
}
