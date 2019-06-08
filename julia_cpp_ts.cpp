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

    std::mutex mtx;
    uv_loop_t* global_event_loop;
    uv_async_t* uv_async_cond;
    std::deque<std::string> tasks;

    std::thread t;

private:
    static julia& julia_instance() {
        static julia julia_instance;
        return julia_instance;
    }

    julia() : t(&julia::julia_main_thread_func, this, std::move(std::unique_lock(mtx))) {}
    ~julia() {
        {
            auto lock = std::unique_lock(mtx);
            running = false;
        };
        notify();
        t.join();
    }

    void notify() { 
        std::cout << "--- notifying" << std::endl;
        uv_async_send(uv_async_cond); uv_stop(global_event_loop); 
        std::cout << "--- notifyed" << std::endl;
    }

    void julia_main_thread_func(std::unique_lock<std::mutex> mtxlock) {

        using namespace std::chrono_literals;

        setenv("JULIA_NUM_THREADS", "4", true);

        jl_init();

        global_event_loop = jl_global_event_loop();

        jl_value_t* _cpp_async_cond_handle = jl_eval_string("const _cpp_async_cond = Base.AsyncCondition(); _cpp_async_cond.handle");
        if (jl_exception_occurred()) {
            std::cout << "!!!!!!!!!! " << jl_typeof_str(jl_exception_occurred()) << std::endl;
        }
        uv_async_cond = reinterpret_cast<uv_async_t*>(jl_unbox_voidpointer(_cpp_async_cond_handle));

        auto setpromise_ptr = std::to_string(reinterpret_cast<std::size_t>(setpromise));
        auto jl_def_setpromise =
            "setpromise(p::Ptr{Cvoid}) = ccall(Ptr{Cvoid}(" + setpromise_ptr + "), Cvoid, (Ptr{Cvoid},), p);";
        jl_eval_string(jl_def_setpromise.c_str());
        if (jl_exception_occurred()) {
            std::cout << "!!!!!!!!!! " << jl_typeof_str(jl_exception_occurred()) << std::endl;
        }

        jl_eval_string("ppp(x) = println(x)");
        if (jl_exception_occurred()) {
            std::cout << "!!!!!!!!!!3 " << jl_typeof_str(jl_exception_occurred()) << std::endl;
        }


        jl_eval_string("println(\"JULIA  START\")");


        while (true) {
            std::string task;
            {
                while (tasks.empty() && running) {
                    std::cout << ">>> waiting" << std::endl;
                    mtxlock.unlock();
                    jl_eval_string("wait(_cpp_async_cond)");
                    if (jl_exception_occurred()) {
                        std::cout << "!!!!!!!!!! " << jl_typeof_str(jl_exception_occurred()) << std::endl;
                    }
                    mtxlock.lock();
                    std::cout << ">>> waked" << std::endl;
                }
                if (!running) {
                    break;
                }
                task = std::move(tasks.front());
                tasks.pop_front();
            }
            mtxlock.unlock();

            jl_eval_string(task.c_str());
            if (jl_exception_occurred()) {
                jl_printf(JL_STDERR, "error during task string evaluation:\n");
                jl_static_show(JL_STDERR, jl_exception_occurred());
                jl_exception_clear();
            }

            mtxlock.lock();
        }

        jl_eval_string("println(\"JULIA END\")");
        jl_atexit_hook(0);
    }

    static void setpromise(std::promise<void>* p) {
        std::cout << "--- setting promise" << std::endl;
        p->set_value();
        std::cout << "--- promise set" << std::endl;
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
            std::cout << "--- emplacing task" << std::endl;
            julia_instance().tasks.emplace_back(std::move(je));
            std::cout << "--- emplaced" << std::endl;
        }
        julia_instance().notify();
        std::cout << "--- waiting future" << std::endl;
        p.get_future().wait();
        std::cout << "--- future got" << std::endl;
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
            std::cout << "--- emplacing task" << std::endl;
            julia_instance().tasks.emplace_back(std::move(je));
            std::cout << "--- emplaced" << std::endl;
        }
        julia_instance().notify();
        std::cout << "--- waiting future" << std::endl;
        p.get_future().wait();
        std::cout << "--- future got" << std::endl;
    }
};


int other_thread() {

    julia::par_eval_string(
        "ppp(\"o1 - $(Threads.threadid())/$(Threads.nthreads())\"); sleep(1); ppp(\"o1\")");

    return 0;
}


int main(int argc, char* argv[]) {

    {

        julia::par_eval_string(
            "ppp(\"m1 - $(Threads.threadid())/$(Threads.nthreads())\"); sleep(1); ppp(\"m1\")");

        std::thread t(other_thread);

        julia::par_eval_string(
            "ppp(\"m2 - $(Threads.threadid())/$(Threads.nthreads())\"); sleep(1); ppp(\"m2\")");

        t.join();
    }

    return 0;
}
