// scheduler.cpp
// Compile with: g++ -std=c++17 -O2 scheduler.cpp main.cpp -pthread -o scheduler

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>
#include <chrono>

struct Job {
    int priority;               // Higher = run earlier
    uint64_t seq;               // Sequence number for FIFO inside same priority
    std::function<void()> func; // The actual task to run

    struct Compare {
        bool operator()(Job const& a, Job const& b) const {
            if (a.priority == b.priority)
                return a.seq > b.seq;  // smaller seq first
            return a.priority < b.priority; // higher priority first
        }
    };
};

class JobScheduler {
public:
    explicit JobScheduler(size_t numThreads = std::thread::hardware_concurrency())
        : stopFlag(false), seqCounter(0), threads(numThreads)
    {
        start(numThreads);
    }

    ~JobScheduler() {
        shutdown();  // Ensure proper cleanup
    }

    template <typename Fn, typename... Args>
    auto submit(int priority, Fn&& f, Args&&... args)
        -> std::future<std::invoke_result_t<Fn, Args...>>
    {
        using Ret = std::invoke_result_t<Fn, Args...>;

        auto bound = std::bind(std::forward<Fn>(f), std::forward<Args>(args)...);
        auto task = std::make_shared<std::packaged_task<Ret()>>(std::move(bound));
        std::future<Ret> fut = task->get_future();

        Job job;
        job.priority = priority;
        job.seq = seqCounter.fetch_add(1);
        job.func = [task]() { (*task)(); };

        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stopFlag)
                throw std::runtime_error("Scheduler stopped. Cannot submit new tasks.");
            queue_.push(std::move(job));
        }

        cv_.notify_one();
        return fut;
    }

    void shutdown() {
        bool expected = false;
        if (!stopFlag.compare_exchange_strong(expected, true))
            return;

        cv_.notify_all();  
        for (auto& t : threads)
            if (t.joinable()) t.join();
    }

private:
    void start(size_t nthreads) {
        for (size_t i = 0; i < nthreads; ++i) {
            threads[i] = std::thread([this]() { worker_loop(); });
        }
    }

    void worker_loop() {
        while (true) {
            Job job;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() { return stopFlag || !queue_.empty(); });

                if (stopFlag && queue_.empty())
                    return;

                job = queue_.top();
                queue_.pop();
            }

            try {
                job.func();
            } catch (...) {
                std::cerr << "Job threw exception\n";
            }
        }
    }

    std::priority_queue<Job, std::vector<Job>, Job::Compare> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stopFlag;
    std::atomic<uint64_t> seqCounter;

    std::vector<std::thread> threads;
};
