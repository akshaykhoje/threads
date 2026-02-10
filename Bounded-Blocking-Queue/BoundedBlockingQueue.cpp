/**
 * @file BoundedBlockingQueue.cpp
 * @brief Implementation of a thread-safe, bounded blocking queue.
 * * This file demonstrates the Producer-Consumer pattern using modern C++ 
 * synchronization primitives like std::mutex and std::condition_variable.
 */

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

/**
 * @class BoundedBlockingQueue
 * @brief A thread-safe queue with a fixed capacity that supports blocking operations.
 * * This class ensures that:
 * - Producers block when the queue is full.
 * - Consumers block when the queue is empty.
 * - All threads can be gracefully shut down.
 */
class BoundedBlockingQueue {
private:
    std::queue<int> buffer;     ///< The underlying queue to store integer data.
    size_t max_capacity;        ///< Maximum capacity of the buffer.
    std::mutex mtx;             ///< Mutex to protect the buffer from concurrent access.

    std::condition_variable not_full;   ///< Signal to producers that space is available.
    std::condition_variable not_empty;  ///< Signal to consumers that data is available.

    std::atomic<bool> is_shutdown{false}; ///< Flag to trigger a safe shutdown across all threads.

public:
    /**
     * @brief Constructs a new BoundedBlockingQueue.
     * @param capacity The maximum number of elements the queue can hold.
     */
    BoundedBlockingQueue(size_t capacity) : max_capacity(capacity) {};

    /**
     * @brief Shuts down the queue and wakes up all blocked threads.
     * * Sets the shutdown flag and notifies all waiting producers and consumers
     * to prevent deadlocks during program termination.
     */
    void shut_down() {
        is_shutdown = true;
        not_full.notify_all();
        not_empty.notify_all();
        return;
    }

    /**
     * @brief Adds an element to the back of the queue.
     * @param val The integer to add.
     * @return true if the item was successfully pushed.
     * @return false if the queue was shut down before or during the push.
     */
    bool push(int val) {
        std::unique_lock<std::mutex> lock(mtx);

        // Wait while full AND not shutting down
        while(buffer.size() >= max_capacity && !is_shutdown) {
            not_full.wait(lock);
        }

        if(is_shutdown) return false;

        buffer.push(val);
        std::cout << "[Producer] Pushed : " << val << " | Size: " << buffer.size() << std::endl;
        
        not_empty.notify_one(); 
        return true;
    }

    /**
     * @brief Removes an element from the front of the queue.
     * @param[out] output_val Reference where the consumed value will be stored.
     * @return true if an item was successfully consumed.
     * @return false if the queue is empty and shut down.
     */
    bool pop(int& output_val) {
        std::unique_lock<std::mutex> lock(mtx);

        // Wait while empty AND not shutting down
        while(buffer.empty() && !is_shutdown) {
            not_empty.wait(lock);
        }

        // Return false only if empty AND shut down
        if(buffer.empty() && is_shutdown) return false;

        output_val = buffer.front();
        buffer.pop();
        std::cout << "[Consumer] : Consumed " << output_val << " | Size : " << buffer.size() << std::endl;

        not_full.notify_one();
        return true;
    }
};

/**
 * @brief Worker function for the Producer thread.
 * @param q Pointer to the shared BoundedBlockingQueue.
 */
void producerTask(BoundedBlockingQueue* q) {
    for(int i=1; i<=30; ++i) {
        if(!q->push(i)) {
            std::cout << "[Producer] Shutdown detected. Stopping production." << std::endl;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    std::cout << "[Producer] Finished all items" << std::endl;
    q->shut_down();
    return;
}

/**
 * @brief Worker function for the Consumer thread.
 * @param q Pointer to the shared BoundedBlockingQueue.
 */
void consumerTask(BoundedBlockingQueue* q) {
    for(int i=1; i<=100; ++i) {
        int val;
        bool pop_status = q->pop(val);
        if(!pop_status) {
            std::cout << "---[ConsumerTask] : SHUTDOWN DETECTED. STOPPING CONSUMPTION---" << std::endl;
            break; 
        } else {
            std::cout << "Consuming value..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    std::cout << "[Consumer] Finished my requirements! Triggering shutdown..." << std::endl;
    q->shut_down();
    return;
}

/**
 * @brief Entry point of the program.
 * Initializes the queue and manages the lifecycle of worker threads.
 */
int main() {
    BoundedBlockingQueue queue(5);

    std::thread producerThread(producerTask, &queue);
    std::thread consumerThread(consumerTask, &queue);

    if(producerThread.joinable())
        producerThread.join();
    if(consumerThread.joinable())
        consumerThread.join();

    std::cout << "All work finished." << std::endl;
    return 0;
}
