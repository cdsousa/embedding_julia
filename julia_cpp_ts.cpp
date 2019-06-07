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

    std::mutex mtx;
    std::condition_variable cond;
    // uvmutex mtx;
    // uvcondition cond;
    uv_loop_t* global_event_loop;
    uv_async_t async;
    uv_pipe_t* pipe_handle;

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


    static void cb(uv_write_t* req, int status) {
        /* Logic which handles the write result */

            std::cout << "===a" << std::endl;
            std::cout << "===b" << std::endl;
    }

    static void async_cb(uv_async_t* handle) { 
            std::cout << "===1" << std::endl;
          // uv_stop(julia_instance().global_event_loop);


            uv_buf_t a[] = {
                { .base = const_cast<char*>("\0"), .len = 1 },
                { .base = const_cast<char*>("2"), .len = 1 }
            };

            uv_write_t req1;

            /* writes "1234" */
            uv_write(&req1, (uv_stream_t*)julia_instance().pipe_handle, a, 1, cb);
        
            std::cout << "===2" << std::endl;
         }

    void julia_main_thread_func(std::unique_lock<std::mutex> lock) {

        using namespace std::chrono_literals;


        uv_setup_args(0, 0);
        libsupport_init();

        setenv("JULIA_NUM_THREADS", "4", true);
        jl_init();



        global_event_loop = jl_global_event_loop();

        uv_async_init(global_event_loop, &async, async_cb);



        jl_eval_string("println(\"JULIA  START\")");


        jl_eval_string("const pe = Base.PipeEndpoint();");
        if (jl_exception_occurred()) {
            std::cout << "!!!!!!!!!!1" << jl_typeof_str(jl_exception_occurred()) << std::endl;
        }

        jl_eval_string("println(pe.handle)");

        jl_value_t* h = jl_eval_string("pe.handle");
        pipe_handle = (uv_pipe_t*)jl_unbox_voidpointer(h);

        auto setpromise_ptr = std::to_string(reinterpret_cast<std::size_t>(setpromise));
        auto jl_def_setpromise =
            "setpromise(p::Ptr{Cvoid}) = ccall(Ptr{Cvoid}(" + setpromise_ptr + "), Cvoid, (Ptr{Cvoid},), p);";
        jl_eval_string(jl_def_setpromise.c_str());
        if (jl_exception_occurred()) {
            std::cout << "!!!!!!!!!!2 " << jl_typeof_str(jl_exception_occurred()) << std::endl;
        }


        jl_eval_string("ppp(x) = println(x)");
        if (jl_exception_occurred()) {
            std::cout << "!!!!!!!!!!3 " << jl_typeof_str(jl_exception_occurred()) << std::endl;
        }


        while (running) {
            std::string task;

            std::cout << ">>> task available?" << std::endl;
            while (tasks.empty() && running) {
                std::cout << ">>> no!" << std::endl;
                // cond.wait(lock);
                std::cout << ">>> waiting" << std::endl;
                lock.unlock();
                // bool active = uv_run(global_event_loop, UV_RUN_ONCE);
                // std::cout << ">>> ..." << std::endl;
                // jl_eval_string("yield()");
                jl_eval_string("wait_readbyte(pe, 0x0)");
                if (jl_exception_occurred()) {
                    std::cout << "!!!!!!!!!! " << jl_typeof_str(jl_exception_occurred()) << std::endl;
                }
                // jl_eval_string("process_events()");
                lock.lock();
                std::cout << ">>> stopped waiting" << std::endl;
                std::cout << ">>> task available?" << std::endl;
            }
            if (!running) {
                break;
            }
            std::cout << ">>> yes!!" << std::endl;

            std::cout << ">>> poping task" << std::endl;
            task = std::move(tasks.front());
            tasks.pop_front();
            std::cout << ">>> poped" << std::endl;

            lock.unlock();

            std::cout << ">>> evaluating string" << std::endl;

            jl_eval_string(task.c_str());
            if (jl_exception_occurred()) {
                jl_printf(JL_STDERR, "error during run:\n");
                jl_static_show(JL_STDERR, jl_exception_occurred());
                jl_exception_clear();
            }

            std::cout << ">>> string evaled" << std::endl;

            lock.lock();
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
    static void notify() {
        auto lock = std::unique_lock(julia_instance().mtx);
        // uv_stop(julia_instance().global_event_loop);
        uv_async_send(&julia_instance().async);

        
    }

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
        std::cout << "--- notifying" << std::endl;
        julia_instance().notify();
        std::cout << "--- notified" << std::endl;
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
        std::cout << "--- notifying" << std::endl;
        julia_instance().notify();
        std::cout << "--- notified - waiting future" << std::endl;
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
