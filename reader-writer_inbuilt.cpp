#include <iostream>
#include <chrono>
#include <thread>
#include <shared_mutex>
#include <vector>
#include <mutex>

std::mutex log_mtx; // global mutex to ensure console output is not interleaved.

class SharedMetaData {
private:
    std::shared_mutex rw_mutex;     // allows multiple readers to read or one writer to write
    int shared_resource = 0;

public:
    // READER METHOD
    void read_data(int thread_id) {
        // // allow concurrent reads, but block if a unique lock is held
        std::shared_lock<std::shared_mutex> reader_lock(rw_mutex);
        {
            // thread-safe logging
            std::lock_guard<std::mutex> log(log_mtx);
            std::cout << "[Reader " << thread_id << "] Reading value: " << shared_resource << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // simulate time to read

        {
            std::lock_guard<std::mutex> log(log_mtx);
            std::cout << "[Reader " << thread_id << "] Finished reading." << std::endl;
        }
        return;
    }   // RAII

    // WRITER METHOD
    void write_data(int thread_id, int new_data) {
        // only one thread can acquire this - blocks all other unique_locks and all shared_locks
        std::unique_lock<std::shared_mutex> writer_lock(rw_mutex);

        {
            std::lock_guard<std::mutex> log(log_mtx);
            std::cout << ">>> [Writer " << thread_id << "] Writing new value: " << new_data << " <<<" << std::endl;
        }

        shared_resource = new_data;     //update the index
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        {
            std::lock_guard<std::mutex> log(log_mtx);
            std::cout << ">>> [Writer " << thread_id << "] Write complete. <<<" << std::endl;
        }
    }   // RAII
};


int main(void) {
    SharedMetaData store;
    std::vector<std::thread> threads;

    // Phase 1 : High concurrency of readers
    threads.emplace_back(&SharedMetaData::write_data, &store, 1, 99);
    for(int i=0; i<=20; i++) {
        threads.emplace_back(&SharedMetaData::read_data, &store, i);
    }
    
    // Phase 2 : Interleaving a writer - writer waits till all active readers finish reading (in phase 1)
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    threads.emplace_back(&SharedMetaData::write_data, &store, 2, 234);
    
    // Phase 3 : Post-write readers - wait until writer finishes
    for(int i=21; i<=30; i++) {
        threads.emplace_back(&SharedMetaData::read_data, &store, i);
    }

    for(int i=31; i<=40; i++) {
        threads.emplace_back(&SharedMetaData::read_data, &store, i);
    }

    for(int i=41; i<=50; i++) {
        threads.emplace_back(&SharedMetaData::read_data, &store, i);
    }

    for(auto& t : threads) {
        if(t.joinable())
            t.join();
    }

    return 0;
}
