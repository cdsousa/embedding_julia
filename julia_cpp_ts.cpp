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
                    std::cout << ">>> waiting cond" << std::endl;
                    cond.wait(lock);
                }
                if (!running) {
                    break;
                }
                std::cout << ">>> poping task" << std::endl;
                task = std::move(tasks.front());
                tasks.pop_front();
                std::cout << ">>> poped" << std::endl;
            }
            std::cout << ">>> evaluating string" << std::endl;

            jl_value_t* val = (jl_value_t*)jl_eval_string(task.c_str());
            if (jl_exception_occurred()) {
                jl_printf(JL_STDERR, "error during run:\n");
                jl_static_show(JL_STDERR, jl_exception_occurred());
                jl_exception_clear();
            } else if (val) {
                jl_static_show(JL_STDOUT, val);
            }
            jl_printf(JL_STDOUT, "\n");

            std::cout << ">>> string evaled" << std::endl;
            uv_run(jl_global_event_loop(), UV_RUN_NOWAIT);
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
                         "println(\"----> $(Threads.threadid())/$(Threads.nthreads())\");"
                         "setpromise(Ptr{Cvoid}(" +
                         std::to_string(reinterpret_cast<std::size_t>(&p)) + "));";
        {
            std::unique_lock<std::mutex> lock(julia_instance().mtx);
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
                         "println(\"----> $(Threads.threadid())/$(Threads.nthreads())\");"
                         ";end);"
                         //  "t.sticky=false;"
                         "schedule(t);";
        {
            std::unique_lock<std::mutex> lock(julia_instance().mtx);
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

    julia::eval_string("println(\"other thread\")");

    return 0;
}


int main(int argc, char* argv[]) {

    std::cout << "Program start" << std::endl;
    {

        julia::par_eval_string("println(\"main thread - 1\")");

        // std::thread t(other_thread);

        // julia::eval_string("println(\"main thread - 2\")");

        // t.join();
    }

    // using namespace std::chrono_literals;
    // std::this_thread::sleep_for(6s);

    std::cout << "Program end" << std::endl;

    return 0;
}
