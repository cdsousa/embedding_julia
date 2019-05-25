#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>


#define JULIA_ENABLE_THREADING
#include <julia.h>


class Worker {
    bool running = true;
    std::thread t;
    std::mutex mtx;
    std::condition_variable cond;
    std::deque<std::function<void()>> tasks;

public:
    Worker() : t{&Worker::threadFunc, this} {}
    ~Worker() {
        {
            std::unique_lock<std::mutex> lock(mtx);
            running = false;
        }
        cond.notify_one();
        t.join();
    }

    template <typename F> auto spawn(const F& f) -> std::packaged_task<decltype(f())()> {
        std::packaged_task<decltype(f())()> task(f);
        {
            std::unique_lock<std::mutex> lock(mtx);
            tasks.push_back([&task] { task(); });
        }
        cond.notify_one();
        return task;
    }

    template <typename F> auto run(const F& f) -> decltype(f()) { return spawn(f).get_future().get(); }

private:
    void threadFunc() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mtx);
                while (tasks.empty() && running) {
                    cond.wait(lock);
                }
                if (!running) {
                    break;
                }
                task = std::move(tasks.front());
                tasks.pop_front();
            }
            task();
        }
    }
};



class Julia {
    Worker worker;

private:
    static Julia& instance() {
        static Julia instance;
        return instance;
    }

    Julia() {
        worker.run([] {
            jl_init();
            jl_eval_string("println(\"JULIA  START\")");
        });
    }
    ~Julia() {
        worker.run([] {
            jl_eval_string("println(\"JULIA END\")");
            jl_atexit_hook(0);
        });
    }

public:
    template <typename F> static auto spawn(const F& f) -> std::packaged_task<decltype(f())()> {
        return instance().worker.spawn(f);
    }
    template <typename F> static auto run(const F& f) -> decltype(f()) { return instance().worker.run(f); }
    static void run(const char* s) {
        return instance().worker.run([&] { jl_eval_string(s); });
    }
};


int other_thread() {
    Julia::run("println(\"other thread\")");
    return 0;
}


int main(int argc, char* argv[]) {

    std::cout << "Program start" << std::endl;
    {

        Julia::run([] { jl_eval_string("println(\"main thread - 1\")"); });

        std::thread t(other_thread);

        Julia::run("println(\"main thread - 2\")");

        t.join();
    }
    std::cout << "Program end" << std::endl;

    return 0;
}
