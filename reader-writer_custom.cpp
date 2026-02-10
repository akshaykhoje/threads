#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>

// global mutex specifically for console output to prevent interleaved logs
std::mutex log_mtx;

class RWLock {
private:
    std::mutex mtx;                      // mutex to protect the internal state variables
    std::condition_variable cv;             // condition variable to sleep/wake threads

    int active_readers = 0;                 // how many threads are currently reading
    int waiting_writers = 0;                // how many threads are currently waiting in line
    bool writer_active = false;             // Is a writer currently in the critical section?

public:
    // READER METHODS

    void lock_read() {
        std::unique_lock<std::mutex> lock(mtx);     // Acquire exlusive access to state
        
        // We stay in the loop as long as writer is active
        // OR as long as a writer is waiting (starvation protection)
        while(writer_active || waiting_writers > 0) {
            cv.wait(lock);                      // release mtx and sleep; re-acquire mtx on wake
        }

        active_readers++;
        return;
    }                                               // RAII

    void unlock_read() {
        std::unique_lock<std::mutex> lock(mtx);     // Acquire exclusive access to state
        active_readers--;                           // One reader finished
        
        // Only notify if the room is now empty
        if(active_readers == 0)
            cv.notify_all();
        return;
    }                                               // RAII

    // WRITER METHODS

    void lock_write() {
        std::unique_lock<std::mutex> lock(mtx);
        waiting_writers++;                          // Tell the system a writer is now "queued"

        // A writer must wait if any other writer is active or if any readers are active
        while(active_readers > 0 || writer_active) {
            cv.wait(lock);                          // release mutex and sleep
        }

        waiting_writers--;                          // we are no longer waiting
        writer_active = true;
        return;
    }                                               // RAII

    void unlock_write() {
        std::unique_lock<std::mutex> lock(mtx);     // Acquire the lock to update state
        writer_active = false;                      // Release the "active" status

        // A writer finishing is a major event, thus, wake everyone because many readers might be waiting
        cv.notify_all();
        return;
    }                                               // RAII
};


// TASK FUNCTIONS

void readerTask(RWLock* rw, int id) {
    // thread-safe logging

    rw->lock_read();
    {
        std::lock_guard<std::mutex> log_lock(log_mtx);  // less overhead than standard mutex
        std::cout << "[Reader " << id << "] Start Reading..." << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Simulate work

    {
        std::lock_guard<std::mutex> log_lock(log_mtx);
        std::cout << "[Reader " << id << "] Finished Reading." << std::endl;
    }
    
    rw->unlock_read();
    return;
}

void writerTask(RWLock* rw, int id) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // small delay to let some readers get started

    rw->lock_write();
    
    {
        std::lock_guard<std::mutex> log_lock(log_mtx);  // less overhead than standard mutex
        std::cout << ">>> [Writer " << id << "] EXCLUSIVE WRITE START <<<" << std::endl;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Simulate write

    {
        std::lock_guard<std::mutex> log_lock(log_mtx);
        std::cout << ">>> [Writer " << id << "] EXCLUSIVE WRITE END <<<" << std::endl;
    }
 
    rw->unlock_write();
    return;
}

int main(void) {
    RWLock rw;
    std::vector<std::thread> threads;   // for dynamic scaling, automated lifecycle mgmt (.join()) at the end, creates the thread object directly inside the vector's memory avoiding unnecessary copying of thread handles

    // start 3 threads
    for(int i=1; i<=50; i++) {
        threads.emplace_back(readerTask, &rw, i);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    // start 2 writers
    for(int i=1; i<=2; i++) {
        threads.emplace_back(writerTask, &rw, i);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    // start 2 more readers (these should be blocked until writers finish)
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
