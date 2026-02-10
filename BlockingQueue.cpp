#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <atomic>

class BlockingQueue {
private:
    std::queue<int> buffer;
    size_t max_capacity;
    std::mutex mtx;

    std::condition_variable not_empty;
    std::condition_variable not_full;

    std::atomic<bool> is_shutdown{false};

public:
    BlockingQueue(size_t capacity) : max_capacity(capacity) {};

    void shutdown() {
        is_shutdown = true;
        not_full.notify_all();  // notify all producer threads
        not_empty.notify_all(); // notify all consumer threads
        return;
    }

    bool push(int val) {
        std::unique_lock<std::mutex> lock(mtx);

        // wait while buffer is full and not shutting down - THE BLOCKING NATURE
        while(buffer.size() >= max_capacity && !is_shutdown) {
            not_full.wait(lock);
        }

        if(is_shutdown)
            return false;
        
        buffer.push(val);
        std::cout << "[Producer] : Pushed : " << val << " | Size : " << buffer.size() << std::endl;

        not_empty.notify_one();
        return true;
    }   // RAII thread exit

    bool pop(int &output_val) {
        std::unique_lock<std::mutex> lock(mtx);

        // wait while buffer is empty and not shutting down - THE BLOCKING NATURE
        while(buffer.size() == 0 && !is_shutdown) {
            not_empty.wait(lock);
        }

        if(buffer.empty() && is_shutdown)
            return false;
        
        output_val = buffer.front();
        buffer.pop();
        std::cout << "[Consumer] : Popped : " << output_val << " | Size : " << buffer.size() << std::endl;
        
        not_full.notify_one();
        return true;
    }   // RAII thread exit
};


void producerTask(BlockingQueue* q) {
    for(int i=1; i<=30; i++) {
        if(!q->push(i)) {
            std::cout << "---[ProducerTask] : SHUTDOWN DETECTED. STOPPING PRODUCTION---" << std::endl;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    std::cout<<"[ProducerTask] : Finished all items" << std::endl;
    q->shutdown();
    return;
}

void consumerTask(BlockingQueue* q) {
    for(int i=1; i<=1000; i++) {
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
    q->shutdown();
    return;
}


int main(void) {
    BlockingQueue queue(5);

    std::thread producer(producerTask, &queue);
    std::thread consumer(consumerTask, &queue);

    if(producer.joinable())
        producer.join();
    if(consumer.joinable())
        consumer.join();
    
    std::cout << "All work finished!" << std::endl;
    return 0;
}
