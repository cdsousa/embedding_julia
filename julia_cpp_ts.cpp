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



class julia {
    bool running = true;
    std::thread t;
    std::mutex mtx;
    std::condition_variable cond;
    std::deque<std::string> tasks;

private:
    static julia& julia_instance() {
        static julia julia_instance;
        return julia_instance;
    }

    julia() : t{&julia::julia_main_thread_func, this} {}
    ~julia() {
        {
            std::unique_lock<std::mutex> lock(mtx);
            running = false;
        };
        cond.notify_one();
        t.join();
    }

    void julia_main_thread_func() {

        using namespace std::chrono_literals;

        // uv_setup_args(0,0);
        // libsupport_init();

        setenv("JULIA_NUM_THREADS", "16", true);
        jl_init();

        jl_eval_string("println(\"JULIA  START\")");

        auto setpromise_ptr = std::to_string(reinterpret_cast<std::size_t>(setpromise));
        auto jl_def_setpromise =
            "setpromise(p::Ptr{Cvoid}) = ccall(Ptr{Cvoid}(" + setpromise_ptr + "), Cvoid, (Ptr{Cvoid},), p);";
        jl_eval_string(jl_def_setpromise.c_str());
        if (jl_exception_occurred()) {
            std::cout << "!!!!!!!!!! " << jl_typeof_str(jl_exception_occurred()) << std::endl;
        }

        while (running) {
            std::string task;
            {
                std::unique_lock<std::mutex> lock(mtx);
                while (tasks.empty() && running) {
                    jl_yield();
                    uv_run(jl_global_event_loop(), UV_RUN_NOWAIT);
                    // std::cout << ">>> waiting cond" << std::endl;
                    cond.wait_for(lock, 100ms);
                }
                if (!running) {
                    break;
                }
                // std::cout << ">>> poping task" << std::endl;
                task = std::move(tasks.front());
                tasks.pop_front();
                // std::cout << ">>> poped" << std::endl;
            }
            // std::cout << ">>> evaluating string" << std::endl;

            jl_eval_string(task.c_str());
            if (jl_exception_occurred()) {
                jl_printf(JL_STDERR, "error during run:\n");
                jl_static_show(JL_STDERR, jl_exception_occurred());
                jl_exception_clear();
            } 

            // std::cout << ">>> string evaled" << std::endl;
        }

        jl_eval_string("println(\"JULIA END\")");
        jl_atexit_hook(0);
    }

    static void setpromise(std::promise<void>* p) {
        // std::cout << ">>> setting promise" << std::endl;
        p->set_value();
        // std::cout << ">>> promise set" << std::endl;
    }

public:
    static void eval_string(const std::string& s) {
        std::promise<void> p;
        std::string je = s +
                         ";"
                         "setpromise(Ptr{Cvoid}(" +
                         std::to_string(reinterpret_cast<std::size_t>(&p)) + "));";
        {
            std::unique_lock<std::mutex> lock(julia_instance().mtx);
            // std::cout << ">>> emplacing task" << std::endl;
            julia_instance().tasks.emplace_back(std::move(je));
            // std::cout << ">>> emplaced" << std::endl;
        }
        // std::cout << ">>> notifying" << std::endl;
        julia_instance().cond.notify_one();
        // std::cout << ">>> notified" << std::endl;
        // std::cout << ">>> waiting future" << std::endl;
        p.get_future().wait();
        // std::cout << ">>> future got" << std::endl;
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
            std::unique_lock<std::mutex> lock(julia_instance().mtx);
            // std::cout << ">>> emplacing task" << std::endl;
            julia_instance().tasks.emplace_back(std::move(je));
            // std::cout << ">>> emplaced" << std::endl;
        }
        // std::cout << ">>> notifying" << std::endl;
        julia_instance().cond.notify_one();
        // std::cout << ">>> notified - waiting future" << std::endl;
        p.get_future().wait();
        // std::cout << ">>> future got" << std::endl;
    }
};


int other_thread() {

    julia::par_eval_string("println(\"o1 - $(Threads.threadid())/$(Threads.nthreads())\"); sleep(3); println(\"o1\")");

    return 0;
}


int main(int argc, char* argv[]) {

    {

        julia::par_eval_string("println(\"m1 - $(Threads.threadid())/$(Threads.nthreads())\"); sleep(3); println(\"m1\")");

        std::thread t(other_thread);

        julia::par_eval_string("println(\"m2 - $(Threads.threadid())/$(Threads.nthreads())\"); sleep(3); println(\"m2\")");

        t.join();
    }

    return 0;
}
