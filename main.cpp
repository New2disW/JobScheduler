// main.cpp
#include <iostream>
#include <chrono>
#include <thread>
#include "scheduler.cpp"

int main() {
    JobScheduler scheduler(4); // 4 worker threads

    auto f1 = scheduler.submit(1, []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::cout << "Low priority job finished\n";
        return 42;
    });

    auto f2 = scheduler.submit(10, []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::cout << "High priority job finished\n";
        return 7;
    });

    auto f3 = scheduler.submit(5, []() {
        std::cout << "Medium priority job finished\n";
        return std::string("OK");
    });

    std::cout << "\nResults:\n";
    std::cout << "High priority job result: " << f2.get() << "\n";
    std::cout << "Low priority job result: " << f1.get() << "\n";
    std::cout << "Medium priority job result: " << f3.get() << "\n";

    scheduler.shutdown();
    return 0;
}
