/**
 * @file CustomRWLock.cpp
 * @brief Manual implementation of a Readers-Writers Lock with Writer Preference.
 * * Unlike std::shared_mutex, this implementation explicitly tracks waiting writers
 * to prevent "Reader Starvation," ensuring writers aren't blocked forever by 
 * a continuous stream of concurrent readers.
 */

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>

/** * @brief Global mutex to prevent console output interleaving.
 * Standard output is not thread-safe; this ensures log messages remain legible.
 */
std::mutex log_mtx;

/**
 * @class RWLock
 * @brief A custom synchronization primitive for the Readers-Writers problem.
 * * This class uses a single std::mutex and std::condition_variable to manage
 * state. It implements a "Writer Preference" policy.
 */
class RWLock {
private:
    std::mutex mtx;                 ///< Protects access to the internal state variables.
    std::condition_variable cv;     ///< Used to block and wake threads based on state changes.

    int active_readers = 0;         ///< Number of threads currently holding a read lock.
    int waiting_writers = 0;        ///< Number of threads waiting to acquire a write lock.
    bool writer_active = false;     ///< Flag indicating if a writer is in the critical section.

public:
    /**
     * @brief Acquires a shared read lock.
     * * Blocks if:
     * 1. A writer is currently active.
     * 2. There are writers waiting in the queue (Starvation Protection).
     */
    void lock_read() {
        std::unique_lock<std::mutex> lock(mtx);
        
        // Wait while a writer is active OR a writer is waiting
        while(writer_active || waiting_writers > 0) {
            cv.wait(lock);
        }

        active_readers++;
    }

    /**
     * @brief Releases the shared read lock.
     * * If this was the last active reader, it notifies all waiting threads 
     * (potentially waking a waiting writer).
     */
    void unlock_read() {
        std::unique_lock<std::mutex> lock(mtx);
        active_readers--;
        
        if(active_readers == 0)
            cv.notify_all();
    }

    /**
     * @brief Acquires an exclusive write lock.
     * * Increments the waiting_writers count to block new readers.
     * Blocks until all active readers and any active writer have finished.
     */
    void lock_write() {
        std::unique_lock<std::mutex> lock(mtx);
        waiting_writers++;

        // Wait while any readers or writers are active
        while(active_readers > 0 || writer_active) {
            cv.wait(lock);
        }

        waiting_writers--;
        writer_active = true;
    }

    /**
     * @brief Releases the exclusive write lock.
     * * Sets writer_active to false and wakes all waiting threads.
     */
    void unlock_write() {
        std::unique_lock<std::mutex> lock(mtx);
        writer_active = false;
        cv.notify_all();
    }
};

/**
 * @brief Simulates a reader's lifecycle.
 * @param rw Pointer to the shared RWLock instance.
 * @param id Unique identifier for the reader thread.
 */
void readerTask(RWLock* rw, int id) {
    rw->lock_read();
    {
        std::lock_guard<std::mutex> log_lock(log_mtx);
        std::cout << "[Reader " << id << "] Start Reading..." << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    {
        std::lock_guard<std::mutex> log_lock(log_mtx);
        std::cout << "[Reader " << id << "] Finished Reading." << std::endl;
    }
    rw->unlock_read();
}

/**
 * @brief Simulates a writer's lifecycle.
 * @param rw Pointer to the shared RWLock instance.
 * @param id Unique identifier for the writer thread.
 */
void writerTask(RWLock* rw, int id) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    rw->lock_write();
    {
        std::lock_guard<std::mutex> log_lock(log_mtx);
        std::cout << ">>> [Writer " << id << "] EXCLUSIVE WRITE START <<<" << std::endl;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    {
        std::lock_guard<std::mutex> log_lock(log_mtx);
        std::cout << ">>> [Writer " << id << "] EXCLUSIVE WRITE END <<<" << std::endl;
    }
    rw->unlock_write();
}

/**
 * @brief Entry point for the custom RWLock demonstration.
 * Creates a mix of reader and writer threads to demonstrate starvation protection.
 */
int main(void) {
    RWLock rw;
    std::vector<std::thread> threads;

    for(int i=1; i<=50; i++) {
        threads.emplace_back(readerTask, &rw, i);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    for(int i=1; i<=2; i++) {
        threads.emplace_back(writerTask, &rw, i);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    for(int i=1001; i<=1100; i++) {
        threads.emplace_back(readerTask, &rw, i);
    }

    for(auto& t : threads) {
        if(t.joinable())
            t.join();
    }

    std::cout << "All metadata operations completed." << std::endl;
    return 0;
}
