#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

// #define JULIA_ENABLE_THREADING
// #include <platform.h>
#include <julia.h>


std::atomic<bool> done(false);


void tf(int (*f)(int)){
    std::cout << ">>>>" << std::endl;

    jl_adopt_thread();

    jl_threadid();

    jl_eval_string("println(20)");
    std::cout << f(123) <<std::endl;

    std::cout << "<<<<" << std::endl;
    done = true;
}


int main(int argc, char* argv[]) {
    static const int N = 10;
    double a[N];

    std::cout << "---------------------" << std::endl;

    jl_init();
    jl_eval_string("println(10)");

    jl_eval_string("f(x::Int)::Int = (println(1111); 2x)");
    int (*f)(int) = (int (*)(int))(jl_unbox_voidpointer(jl_eval_string("@cfunction(f, Int, (Int,))")));

    // size_t __excstack_state = jl_excstack_state();
    // f(123);

    std::thread t(tf, f);
    std::cout << "--- 1" << std::endl;
    std::cout << "--- 2" << std::endl;

    jl_eval_string("println(Threads.nthreads())");

    jl_eval_string("println(11)");

    while(!done){
        jl_eval_string("sleep(0.001)");
    }
    std::cout << "join..." << std::endl;
    t.join();

    jl_atexit_hook(0);

    return 0;
}
