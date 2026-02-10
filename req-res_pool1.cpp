#include <iostream>
#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <future> // New: Required for packaged_task and future

typedef std::function<void()> Task;

class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<Task> tasks;
    std::mutex queue_mtx;
    std::condition_variable cv;
    std::atomic<bool> stop;

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
            workers.emplace_back(&ThreadPool::workerLoop, this);
        }
    }

    /**
     * The UPGRADED Enqueue
     * T is the return type of the function f
     */
    template<class F>
    auto enqueue(F f) -> std::future<decltype(f())> {
        // 1. Determine the return type of the function
        using ReturnType = decltype(f());

        // 2. Wrap the function in a packaged_task. 
        // We use a shared_ptr so we can copy the task into the void() queue safely.
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(f);

        // 3. Get the "Future" (the ticket) from the task before we send it away
        std::future<ReturnType> res = task->get_future();

        // 4. Wrap the packaged_task in a void() wrapper so the worker can run it
        {
            std::unique_lock<std::mutex> lock(queue_mtx);
            tasks.emplace([task]() { (*task)(); });
        }

        cv.notify_one();
        return res; // Return the future to the user
    }

    ~ThreadPool() {
        stop = true;
        cv.notify_all();
        for (std::thread &worker : workers) {
            if (worker.joinable()) worker.join();
        }
    }
};

bool uploadFile(std::string name) {
    std::cout << "Uploading " << name << "..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return true; // Return success
}

int main() {
    ThreadPool pool(4);

    // Enqueue tasks and get "Futures" back
    std::future<bool> f1 = pool.enqueue(std::bind(uploadFile, "Agreement.pdf"));
    std::future<bool> f2 = pool.enqueue(std::bind(uploadFile, "Receipt.pdf"));

    // Do other work while the upload happens in the background...
    std::cout << "Doing other work..." << std::endl;

    // Now, check the results. This will BLOCK until the tasks are done.
    if (f1.get() && f2.get()) {
        std::cout << "All files uploaded successfully!" << std::endl;
    }

    return 0;
}
