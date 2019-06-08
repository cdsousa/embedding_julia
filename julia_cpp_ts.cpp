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
extern "C"{
    JL_DLLEXPORT jl_task_t *jl_task_get_next(jl_value_t *getsticky);
}



class julia {
    bool running = true;

    std::mutex mtx;
    uv_loop_t* global_event_loop;
    uv_async_t async;

    std::deque<std::string> tasks;

    std::thread t;

private:
    static julia& julia_instance() {
        static julia julia_instance;
        return julia_instance;
    }

    julia() : t(&julia::julia_main_thread_func, this, std::move(std::unique_lock(mtx))) {
    }
    ~julia() {
        {
            auto lock = std::unique_lock(mtx);
            running = false;
        };
        notify();
        t.join();
    }


    static void async_cb(uv_async_t* handle) { 
            std::cout << "=== in" << std::endl;

            while(true){
                std::string task;
                {
                    std::unique_lock<std::mutex> lock;

                    std::cout << ">>> task available?" << std::endl;
                    if(julia_instance().tasks.empty() || !julia_instance().running) {
                          std::cout << ">>> no!" << std::endl;
                        break;
                    }
                    std::cout << ">>> yes!" << std::endl;

                    std::cout << ">>> poping task" << std::endl;
                    task = std::move(julia_instance().tasks.front());
                    julia_instance().tasks.pop_front();
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

            if(!julia_instance().running){
                std::cout << ">>> stopping " << std::endl;
                // julia_instance().global_event_loop->stop_flag = 1;
            //    enq_work(current_task()
            //     // jl_wakeup_thread(0);
                
                // uv_stop(uv_default_loop());

            // jl_eval_string("push!(Base.Workqueues[Threads.threadid()], @task(nothing))");

            //    if (jl_exception_occurred()) {
            //         std::cout << "!!!!!!!!!!0" << jl_typeof_str(jl_exception_occurred()) << std::endl;
            //      }

                // jl_try_deliver_sigint();


            //    raise(SIGTSTP);

               jl_eval_string("fff() = push!(Base.Workqueues[Threads.threadid()], @task(println(1111111)))");
               if (jl_exception_occurred()) {
                    std::cout << "!!!!!!!!!!0" << jl_typeof_str(jl_exception_occurred()) << std::endl;
                 }


                jl_function_t* func = jl_get_function(jl_main_module, "fff");

                jl_call0(func);
                jl_call0(func);
                jl_call0(func);
                jl_call0(func);
                jl_call0(func);
                jl_call0(func);

                jl_eval_string("notify(ccc)");

            //    uv_stop(uv_default_loop());
            }
        
            std::cout << "=== out" << std::endl;
        }

    void julia_main_thread_func(std::unique_lock<std::mutex> lock) {

        using namespace std::chrono_literals;


        uv_setup_args(0, 0);
        libsupport_init();

        setenv("JULIA_NUM_THREADS", "1", true);

        jl_init();


        global_event_loop = jl_global_event_loop();

        uv_async_init(global_event_loop, &async, async_cb);



        jl_eval_string("println(\"JULIA  START\")");




        jl_eval_string("const ccc = Condition()");
        if (jl_exception_occurred()) {
            std::cout << "!!!!!!!!!!3 " << jl_typeof_str(jl_exception_occurred()) << std::endl;
        }



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
     
        std::cout << ">>> waiting" << std::endl;

                lock.unlock();

                    jl_eval_string("lock(ccc); wait(ccc); unlock(ccc)");


                // while(running){
                //     // jl_eval_string("yield()");
                //      jl_cpu_pause();
                //     bool active = uv_run(global_event_loop, UV_RUN_ONCE);
                // }


                // jl_value_t* getsticky = jl_eval_string("mygettask() = Base.trypoptask(Base.Workqueues[Threads.threadid()]); mygettask");
                // if (jl_exception_occurred()) {
                //     std::cout << "!!!!!!!!!!aaa" << jl_typeof_str(jl_exception_occurred()) << std::endl;
                // }
                // jl_eval_string("ccall(:jl_task_get_next, Any, (Any,), mygettask)");
                // if (jl_exception_occurred()) {
                //     std::cout << "!!!!!!!!!!bbbb" << jl_typeof_str(jl_exception_occurred()) << std::endl;
                // }

                // jl_value_t* getsticky = jl_get_function(jl_main_module, "mygettask");
                // jl_task_get_next(getsticky);




                lock.lock();

        if (jl_exception_occurred()) {
            std::cout << "!!!!!!!!!!4" << jl_typeof_str(jl_exception_occurred()) << std::endl;
        }


        std::cout << ">>> stopped waiting" << std::endl;

        jl_eval_string("println(\"JULIA END\")");
        jl_atexit_hook(0);
    }

    static void setpromise(std::promise<void>* p) {
        std::cout << "--- setting promise" << std::endl;
        p->set_value();
        std::cout << "--- promise set" << std::endl;
    }

public:
    static void waitinit() {
            auto lock = std::unique_lock(julia_instance().mtx);
    }

    static void notify() {
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

    julia::waitinit();


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
