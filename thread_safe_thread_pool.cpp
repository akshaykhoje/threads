#include <iostream>
#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <utility>

// --- 1. THE THREAD-SAFE RESULT QUEUE ---
// This acts as the "Inbox" for the main thread.
template <typename T>
class ResultQueue {
private:
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cv;

public:
    void push(T result) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push(result);
        }
        cv.notify_one(); // Wake up the main thread
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mtx);
        // Wait until there is at least one result in the inbox
        cv.wait(lock, [this] { return !this->queue.empty(); });
        T val = std::move(this->queue.front());
        queue.pop();
        return val;
    }
};

// --- 2. THE THREAD POOL ---
typedef std::function<void()> Task;

class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<Task> tasks;
    std::mutex queue_mtx;
    std::condition_variable cv;
    std::atomic<bool> stop;

    // Member function for the worker loop
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
            // Member function pointer syntax
            workers.emplace_back(&ThreadPool::workerLoop, this);
        }
    }

    void enqueue(Task t) {
        {
            std::unique_lock<std::mutex> lock(queue_mtx);
            tasks.push(std::move(t));
        }
        cv.notify_one();
    }

    ~ThreadPool() {
        stop = true;
        cv.notify_all();
        for (std::thread &worker : workers) {
            if (worker.joinable()) worker.join();
        }
    }
};

// --- 3. THE ACTUAL LOGIC (Functor Style) ---
// Since we want to avoid lambdas, we use a struct that stores the 
// result queue pointer and the data to process.
struct IsPrimeCalculator {
    int n;
    ResultQueue<std::pair<bool, int>>* outputQueue;

    // The logic inside the operator()
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
        // Push the result to the shared queue
        outputQueue->push({prime, n});
    }
};

// --- 4. THE ORCHESTRATOR ---
int main() {
    int core_count = std::thread::hardware_concurrency();
    ThreadPool pool(core_count);
    ResultQueue<std::pair<bool, int>> results;

    std::cout << "System started with " << core_count << " workers.\n";

    // Submitting 500 tasks
    for (int i = 1; i <= 500; ++i) {
        // We initialize the struct with the number and the result queue address
        IsPrimeCalculator taskObj;
        taskObj.n = i;
        taskObj.outputQueue = &results;
        
        // The pool takes the functor. std::function<void()> handles it perfectly.
        pool.enqueue(taskObj);
    }

    // Collecting 500 results out-of-order
    for (int i = 1; i <= 500; ++i) {
        std::pair<bool, int> res = results.pop();
        if (res.first) {
            // Using log_mtx logic (skipped here for brevity but same as before)
            std::cout << "[Main] Received Result: " << res.second << " is PRIME\n";
        }
    }

    std::cout << "All tasks processed asynchronously!\n";
    return 0;
}
