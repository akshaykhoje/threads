/**
 * @file ReaderWriterLock.cpp
 * @brief Demonstration of Thread-Safe Data Access using Readers-Writers Lock.
 * * This file illustrates the use of std::shared_mutex to allow multiple threads 
 * to read data simultaneously while ensuring that writing is an exclusive operation.
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <shared_mutex>
#include <vector>
#include <mutex>

/** * @brief Global mutex to prevent console output interleaving.
 * * Standard output (std::cout) is not inherently thread-safe for atomic operations. 
 * This mutex ensures that logs from different threads do not garble each other.
 */
std::mutex log_mtx; 

/**
 * @class SharedMetaData
 * @brief Manages a shared resource with high-concurrency read access.
 * * Uses std::shared_mutex to solve the Readers-Writers problem.
 * - **Readers** use std::shared_lock (shared ownership).
 * - **Writers** use std::unique_lock (exclusive ownership).
 */
class SharedMetaData {
private:
    std::shared_mutex rw_mutex;     ///< The core R/W lock allowing multiple readers or one writer.
    int shared_resource = 0;        ///< The actual data being protected.

public:
    /**
     * @brief Reads the shared data concurrently.
     * * Multiple reader threads can execute this method at the same time,
     * provided no writer holds an exclusive lock.
     * * @param thread_id Unique identifier for the reader thread.
     */
    void read_data(int thread_id) {
        // Acquire shared ownership
        std::shared_lock<std::shared_mutex> reader_lock(rw_mutex);
        
        {
            // Thread-safe logging for the start of the read
            std::lock_guard<std::mutex> log(log_mtx);
            std::cout << "[Reader " << thread_id << "] Reading value: " << shared_resource << std::endl;
        }

        // Simulate a time-consuming read operation
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); 

        {
            // Thread-safe logging for the end of the read
            std::lock_guard<std::mutex> log(log_mtx);
            std::cout << "[Reader " << thread_id << "] Finished reading." << std::endl;
        }
    }

    /**
     * @brief Updates the shared data exclusively.
     * * Blocks all incoming readers and waits for existing readers to finish
     * before modifying the shared_resource.
     * * @param thread_id Unique identifier for the writer thread.
     * @param new_data The new value to store in the resource.
     */
    void write_data(int thread_id, int new_data) {
        // Acquire exclusive ownership
        std::unique_lock<std::shared_mutex> writer_lock(rw_mutex);

        {
            std::lock_guard<std::mutex> log(log_mtx);
            std::cout << ">>> [Writer " << thread_id << "] Writing new value: " << new_data << " <<<" << std::endl;
        }

        shared_resource = new_data;     // Update the resource
        
        // Simulate a heavy write operation
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        {
            std::lock_guard<std::mutex> log(log_mtx);
            std::cout << ">>> [Writer " << thread_id << "] Write complete. <<<" << std::endl;
        }
    }
};

/**
 * @brief Main execution logic.
 * * Orchestrates a high-concurrency scenario in three phases:
 * 1. Initial writer followed by a burst of 20 readers.
 * 2. An interleaved writer that waits for the burst of readers to clear.
 * 3. Subsequent bursts of readers (30 total) that must wait for the writer to finish.
 */
int main(void) {
    SharedMetaData store;
    std::vector<std::thread> threads;

    // Phase 1 : Initial Write and High concurrency of readers
    threads.emplace_back(&SharedMetaData::write_data, &store, 1, 99);
    for(int i=0; i<=20; i++) {
        threads.emplace_back(&SharedMetaData::read_data, &store, i);
    }
    
    // Phase 2 : Interleaving a writer
    // This writer will wait until all active readers from Phase 1 finish their 200ms sleep.
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    threads.emplace_back(&SharedMetaData::write_data, &store, 2, 234);
    
    // Phase 3 : Post-write readers
    // These readers will block until Writer 2 releases the unique_lock.
    for(int i=21; i<=30; i++) {
        threads.emplace_back(&SharedMetaData::read_data, &store, i);
    }

    for(int i=31; i<=40; i++) {
        threads.emplace_back(&SharedMetaData::read_data, &store, i);
    }

    for(int i=41; i<=50; i++) {
        threads.emplace_back(&SharedMetaData::read_data, &store, i);
    }

    // Join all threads to ensure clean termination
    for(auto& t : threads) {
        if(t.joinable())
            t.join();
    }

    return 0;
}
