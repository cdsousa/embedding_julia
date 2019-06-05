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

class uvmutex {
    uv_mutex_t _m;

public:
    uvmutex() { uv_mutex_init(&_m); }
    ~uvmutex() { uv_mutex_destroy(&_m); }
    uv_mutex_t* uv_mutex() { return &_m; }
    void lock() { uv_mutex_lock(&_m); }
    bool try_lock() { return uv_mutex_trylock(&_m) == 0; }
    void unlock() { uv_mutex_unlock(&_m); }
};

class uvcondition {
    uv_cond_t _c;

public:
    uvcondition() { uv_cond_init(&_c); }
    ~uvcondition() { uv_cond_destroy(&_c); }
    void wait(std::unique_lock<uvmutex>& lock) { uv_cond_wait(&_c, lock.mutex()->uv_mutex()); }
    void notify_one() { uv_cond_signal(&_c); }
};

class julia {
    bool running = true;
    std::thread t;

    std::mutex mtx;
    std::condition_variable cond;
    // uvmutex mtx;
    // uvcondition cond;

    std::deque<std::string> tasks;

private:
    static julia& julia_instance() {
        static julia julia_instance;
        return julia_instance;
    }

    julia() : t{&julia::julia_main_thread_func, this} {}
    ~julia() {
        {
            auto lock = std::unique_lock(mtx);
            running = false;
        };
        cond.notify_one();
        t.join();
    }

    void julia_main_thread_func() {

        using namespace std::chrono_literals;

        uv_setup_args(0, 0);
        libsupport_init();

        setenv("JULIA_NUM_THREADS", "4", true);
        jl_init();



        jl_eval_string("println(\"JULIA  START\")");



        auto setpromise_ptr = std::to_string(reinterpret_cast<std::size_t>(setpromise));
        auto jl_def_setpromise =
            "setpromise(p::Ptr{Cvoid}) = ccall(Ptr{Cvoid}(" + setpromise_ptr + "), Cvoid, (Ptr{Cvoid},), p);";
        jl_eval_string(jl_def_setpromise.c_str());
        if (jl_exception_occurred()) {
            std::cout << "!!!!!!!!!! " << jl_typeof_str(jl_exception_occurred()) << std::endl;
        }


        jl_eval_string("ppp(x) = println(x)");
        if (jl_exception_occurred()) {
            std::cout << "!!!!!!!!!! " << jl_typeof_str(jl_exception_occurred()) << std::endl;
        }


        while (running) {
            std::string task;
            {
                auto lock = std::unique_lock(mtx);
                std::cout << ">>> checking task available" << std::endl;
                while (tasks.empty() && running) {
                    // cond.wait(lock);
                    std::cout << ">>> waiting" << std::endl;
                    lock.unlock();
                    // uv_loop_t *loop = jl_global_event_loop();
                    // loop->stop_flag = 0;
                    // bool active = uv_run(loop, UV_RUN_NOWAIT);
                    jl_eval_string("wait()");
                    lock.lock();
                }
                if (!running) {
                    break;
                }
                std::cout << ">>> task available" << std::endl;
                std::cout << ">>> poping task" << std::endl;
                task = std::move(tasks.front());
                tasks.pop_front();
                std::cout << ">>> poped" << std::endl;
            }
            std::cout << ">>> evaluating string" << std::endl;

            jl_eval_string(task.c_str());
            if (jl_exception_occurred()) {
                jl_printf(JL_STDERR, "error during run:\n");
                jl_static_show(JL_STDERR, jl_exception_occurred());
                jl_exception_clear();
            }

            std::cout << ">>> string evaled" << std::endl;
        }

        jl_eval_string("println(\"JULIA END\")");
        jl_atexit_hook(0);
    }

    static void setpromise(std::promise<void>* p) {
        std::cout << ">>> setting promise" << std::endl;
        p->set_value();
        std::cout << ">>> promise set" << std::endl;
    }

public:
    static void eval_string(const std::string& s) {
        std::promise<void> p;
        std::string je = s +
                         ";"
                         "setpromise(Ptr{Cvoid}(" +
                         std::to_string(reinterpret_cast<std::size_t>(&p)) + "));";
        {
            auto lock = std::unique_lock(julia_instance().mtx);
            std::cout << ">>> emplacing task" << std::endl;
            julia_instance().tasks.emplace_back(std::move(je));
            std::cout << ">>> emplaced" << std::endl;
        }
        std::cout << ">>> notifying" << std::endl;
        julia_instance().cond.notify_one();
        std::cout << ">>> notified" << std::endl;
        std::cout << ">>> waiting future" << std::endl;
        p.get_future().wait();
        std::cout << ">>> future got" << std::endl;
    }

    static void par_eval_string(const std::string& s) {
        std::promise<void> p;
        std::string je = std::string("t = @task(begin;") +  //
                         s +
                         ";"
                         "setpromise(Ptr{Cvoid}(" +
                         std::to_string(reinterpret_cast<std::size_t>(&p)) +
                         "));"
                         ";end);"
                         //   "t.sticky=false;"
                         "schedule(t);";
        {
            auto lock = std::unique_lock(julia_instance().mtx);
            std::cout << ">>> emplacing task" << std::endl;
            julia_instance().tasks.emplace_back(std::move(je));
            std::cout << ">>> emplaced" << std::endl;
        }
        std::cout << ">>> notifying" << std::endl;
        julia_instance().cond.notify_one();
        std::cout << ">>> notified - waiting future" << std::endl;
        p.get_future().wait();
        std::cout << ">>> future got" << std::endl;
    }
};


int other_thread() {

    julia::par_eval_string(
        "ppp(\"o1 - $(Threads.threadid())/$(Threads.nthreads())\"); sleep(3); ppp(\"o1\")");

    return 0;
}


int main(int argc, char* argv[]) {

    {

        julia::par_eval_string(
            "ppp(\"m1 - $(Threads.threadid())/$(Threads.nthreads())\"); sleep(3); ppp(\"m1\")");

        std::thread t(other_thread);

        julia::par_eval_string(
            "ppp(\"m2 - $(Threads.threadid())/$(Threads.nthreads())\"); sleep(3); ppp(\"m2\")");

        t.join();
    }

    return 0;
}
