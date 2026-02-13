#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

class EvenOddPrinter {
private:
    std::mutex mtx;
    std::condition_variable cv;
    int count = 1;              // shared resource
    int max_count;
public:
    EvenOddPrinter(int num) : max_count(num) {}

    void printOdd() {
        while(true) {
            std::unique_lock<std::mutex> lock(mtx);
            
            while(count % 2 == 0 && count <= max_count) {
                cv.wait(lock);
            }

            if(count > max_count) {
                cv.notify_all();
                break;
            }

            std::cout << "Odd: " << count << std::endl;
            count++;

            cv.notify_all();
        }
    }

    void printEven() {
        while(true) {
            std::unique_lock<std::mutex> lock(mtx);
            
            while(count % 2 != 0 && count <= max_count) {
                cv.wait(lock);
            }

            if(count > max_count) {
                cv.notify_all();
                break;
            }

            std::cout << "Even: " << count << std::endl;
            count++;

            cv.notify_all();
        }
    }
};

int main(void) {
    EvenOddPrinter printer(1000);

    std::thread t1(&EvenOddPrinter::printOdd, &printer);
    std::thread t2(&EvenOddPrinter::printEven, &printer);

    t1.join();
    t2.join();

    std::cout << "Finished printing numbers up to 10." << std::endl;

    return 0;
}
