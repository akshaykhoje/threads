#include <iostream>
#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <chrono>

// Using a type alias for a Task: a callable that takes nothing and returns nothing
typedef std::function<void()> Task;

// Global log mutex to keep our CLI output clean
std::mutex log_mtx;

class ThreadPool {
private:
    std::vector<std::thread> workers;   // The fixed-size pool of persistent threads
    std::queue<Task> tasks;            // The buffer (queue) for incoming jobs
    
    std::mutex queue_mtx;             // Protects access to the task queue
    std::condition_variable cv;       // Signals workers when a task is added or pool stops
    std::atomic<bool> stop;           // Atomic flag to ensure thread-safe shutdown

    // This is the MEMBER FUNCTION each thread runs. 
    // It's private because it's only for the pool's internal use.
    void workerLoop() {
        while (true) {
            Task task; // Prepare an empty task container

            // --- CRITICAL SECTION START ---
            {
                std::unique_lock<std::mutex> lock(this->queue_mtx);
                
                // Stay asleep as long as there's no work and no shutdown signal
                while (!this->stop && this->tasks.empty()) {
                    this->cv.wait(lock);
                }

                // If shutdown is triggered and all tasks are done, terminate thread
                if (this->stop && this->tasks.empty()) {
                    return;
                }

                // Retrieve the task (using move for efficiency)
                task = std::move(this->tasks.front());
                this->tasks.pop();
            }
            // --- CRITICAL SECTION END (lock released here) ---

            // Execute the task outside the lock to allow other threads to access the queue
            if (task) {
                task(); 
            }
        }
    }

public:
    // Constructor: Initializes the pool with a specific number of threads
    ThreadPool(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            // Start thread using Member Function Pointer syntax:
            // &Class::Method, then 'this' to tell it which object instance to use.
            workers.emplace_back(&ThreadPool::workerLoop, this);
        }
    }

    // Submit a task to the queue
    void enqueue(Task t) {
        {
            std::unique_lock<std::mutex> lock(queue_mtx);
            tasks.push(std::move(t));
        }
        cv.notify_one(); // Wake up exactly one worker to handle this task
    }

    // Destructor: Ensures graceful shutdown
    ~ThreadPool() {
        stop = true;        // Signal all threads to wrap up
        cv.notify_all();    // Wake up everyone waiting on the condition variable
        
        for (std::thread &worker : workers) {
            if (worker.joinable()) {
                worker.join(); // Wait for each thread to finish its current task
            }
        }
    }
};

// --- CLI Simulation Tasks ---

void dataProcessingTask(int id) {
    {
        std::lock_guard<std::mutex> lock(log_mtx);
        std::cout << "[Task " << id << "] is being processed by a worker..." << std::endl;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300)); // Simulate CPU work
}

int main() {
    // 1. Initialize ThreadPool with 12 persistent workers
    ThreadPool pool(12);

    {
        std::lock_guard<std::mutex> lock(log_mtx);
        std::cout << "--- System Initialized with 12 Worker Threads ---" << std::endl;
    }

    // 2. Submit 1000 tasks to the pool
    // Notice how the 12 workers will "share" the 1000 tasks
    for (int i = 1; i <= 1000; ++i) {
        // std::bind wraps our function so it fits the Task signature: void()
        pool.enqueue(std::bind(dataProcessingTask, i));
    }

    // 3. Keep main alive long enough to see results (or join happens in destructor)
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    {
        std::lock_guard<std::mutex> lock(log_mtx);
        std::cout << "Main finished. Destructor will now clean up the pool." << std::endl;
    }
    return 0;
}
