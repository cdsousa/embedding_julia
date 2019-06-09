#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>
#include <variant>
#include <vector>


#define JULIA_ENABLE_THREADING
#include <julia.h>


class julia {


    bool running = true;

    std::mutex mtx;
    uv_loop_t* global_event_loop;
    uv_async_t* uv_async_cond;
    typedef std::variant<std::function<void()>, std::string> var_t;
    std::deque<var_t> tasks;

    std::thread t;

    julia() : t(&julia::julia_main_thread_func, this, std::move(std::unique_lock(mtx))) {}
    ~julia() {
        {
            auto lock = std::unique_lock(mtx);
            running = false;
        };
        notify();
        t.join();
    }

    void notify() { uv_async_send(uv_async_cond); }

    void julia_main_thread_func(std::unique_lock<std::mutex> mtxlock) {

        using namespace std::chrono_literals;

        setenv("JULIA_NUM_THREADS", "4", true);

        jl_init();

        global_event_loop = jl_global_event_loop();

        jl_value_t* _cpp_async_cond_handle =
            jl_eval_string("const _cpp_async_cond = Base.AsyncCondition(); _cpp_async_cond.handle");

        uv_async_cond = reinterpret_cast<uv_async_t*>(jl_unbox_voidpointer(_cpp_async_cond_handle));

        auto setpromise_ptr = std::to_string(reinterpret_cast<std::size_t>(setpromise));
        auto jl_def_setpromise =
            "setpromise(p::Ptr{Cvoid}) = ccall(Ptr{Cvoid}(" + setpromise_ptr + "), Cvoid, (Ptr{Cvoid},), p);";
        jl_eval_string(jl_def_setpromise.c_str());

        jl_eval_string("ppp(x) = println(x)");

        if (jl_exception_occurred()) {
            std::cerr << "Julia exception on initialization: " << jl_typeof_str(jl_exception_occurred())
                      << std::endl;
            return;
        }


        jl_eval_string("println(\"JULIA  START\")");


        var_t task;

        while (true) {
            {
                while (tasks.empty() && running) {
                    mtxlock.unlock();
                    jl_eval_string("wait(_cpp_async_cond)");
                    if (jl_exception_occurred()) {
                        std::cerr << "Julia exception on waiting: " << jl_typeof_str(jl_exception_occurred())
                                  << std::endl;
                        return;
                    }
                    mtxlock.lock();
                }
                if (!running) {
                    break;
                }
                task = std::move(tasks.front());
                tasks.pop_front();
            }
            mtxlock.unlock();

            if (auto str = std::get_if<std::string>(&task)) {
                jl_eval_string(str->c_str());
                if (jl_exception_occurred()) {
                    jl_printf(JL_STDERR, "error during task string evaluation:\n");
                    jl_static_show(JL_STDERR, jl_exception_occurred());
                    jl_exception_clear();
                }
            } else if (auto pf = std::get_if<std::function<void()>>(&task); auto& f = *pf) {
                f();
                // auto str_p_callfunction = std::to_string(reinterpret_cast<std::size_t>(&callfunction));
                // auto str_p_f = std::to_string(reinterpret_cast<std::size_t>(&f));
                // auto s = "ccall(Ptr{Cvoid}(" + str_p_callfunction + "), Cvoid, (Ptr{Cvoid},), Ptr{Cvoid}("
                // +
                //          str_p_f + "));";
                // std::cout << s << std::endl;
                // jl_eval_string(s.c_str());
                // if (jl_exception_occurred()) {
                //     jl_printf(JL_STDERR, "error during task string evaluation:\n");
                //     jl_static_show(JL_STDERR, jl_exception_occurred());
                //     jl_exception_clear();
                // }
            }

            mtxlock.lock();
        }

        jl_eval_string("println(\"JULIA END\")");
        jl_atexit_hook(0);
    }

    static void setpromise(std::promise<void>* p) { p->set_value(); }
    static void callfunction(std::function<void()>* f) { (*f)(); }

    static julia& julia_instance() {
        static julia julia_instance;
        return julia_instance;
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
            julia_instance().tasks.emplace_back(std::move(je));
        }
        julia_instance().notify();
        p.get_future().wait();
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
            julia_instance().tasks.emplace_back(std::move(je));
        }
        julia_instance().notify();
        p.get_future().wait();
    }

    template <typename F> static auto run(const F& f) -> decltype(f()) {
        std::packaged_task<decltype(f())()> task(f);
        {
            auto lock = std::unique_lock<std::mutex>(julia_instance().mtx);
            julia_instance().tasks.emplace_back([&task] { task(); });
        }
        julia_instance().notify();
        return task.get_future().get();
    }
};


int other_thread() {

    julia::par_eval_string(
        "ppp(\"o1 - $(Threads.threadid())/$(Threads.nthreads())\"); sleep(3); ppp(\"o1\")");

    return 0;
}


int main(int argc, char* argv[]) {

    julia::run([] { jl_eval_string("println(123456789)"); });

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
