#include <chrono>
#include <future>
#include <iostream>
#include <thread>



#define JULIA_ENABLE_THREADING
#include <julia.h>
#include <platform.h>

void setpromise(std::promise<void>* p) {
    std::cout << "JJJJJJJJ  " << std::endl;
    p->set_value();
}


int main(int argc, char* argv[]) {

    std::cout << "---------------------" << std::endl;

    setenv("JULIA_NUM_THREADS", "16", true);
    jl_init();
    jl_eval_string("println(\">>> julia in <<<\")");


    //////////////////////////


    jl_function_t* func = jl_get_function(jl_base_module, "sqrt");
    jl_value_t* argument = jl_box_float64(2.0);
    jl_value_t* ret = jl_call1(func, argument);


    //////////////////////////

    auto setpromise_ptr = std::to_string(reinterpret_cast<std::size_t>(setpromise));
    auto jl_def_setpromise =
        "setpromise(p::Ptr{Cvoid}) = ccall(Ptr{Cvoid}(" + setpromise_ptr + "), Cvoid, (Ptr{Cvoid},), p);";
    jl_eval_string(jl_def_setpromise.c_str());
    if (jl_exception_occurred()) {
        std::cout << "!!!!!!!!!! " << jl_typeof_str(jl_exception_occurred()) << std::endl;
    }


    std::promise<void> p;



    // std::string je = "println(\"----> $(Threads.threadid())/$(Threads.nthreads())\");"
    //                  "setpromise(Ptr{Cvoid}(" +
    //                  std::to_string(reinterpret_cast<std::size_t>(&p)) +
    //                  "));"
    //                  "println(\"----<\");";

    std::string je = "t = @task(begin;"
                     "setpromise(Ptr{Cvoid}(" +
                     std::to_string(reinterpret_cast<std::size_t>(&p)) +
                     "));"
                     "println(\"----> $(Threads.threadid())/$(Threads.nthreads())\");"
                     "println(\"----<\");"
                     ";end);"
                     "t.sticky=true;"
                     "schedule(t);"
                     "yield();";

    // std::cout << je << std::endl;

    jl_eval_string(je.c_str());
    if (jl_exception_occurred()) {
        std::cout << "!!!!!!!!!! " << jl_typeof_str(jl_exception_occurred()) << std::endl;
    }

    // auto t = std::thread([&]() {
    //     using namespace std::chrono_literals;
    //     std::this_thread::sleep_for(3s);
    //     setpromise(&p);
    // });

    std::cout << "Waitting promise..." << std::endl;
    p.get_future().get();
    std::cout << "Promise got." << std::endl;

    // t.join();

    if (jl_exception_occurred()) {
        std::cout << "!!!!!!!!!! " << jl_typeof_str(jl_exception_occurred()) << std::endl;
    }



    //////////////////////////


    jl_eval_string("println(\">>> julia out <<<\")");
    jl_atexit_hook(0);

    std::cout << "---------------------" << std::endl;

    return 0;
}
