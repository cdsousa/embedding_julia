#include <iostream>
#include <thread>
#include <chrono>



#define JULIA_ENABLE_THREADING
#include <platform.h>
#include <julia.h>

extern "C"{
    void jl_init_threadtls(int16_t tid);
}


void tf(int (*f)(int)){
    std::cout << ">>>>" << std::endl;

    jl_threadid();

    // jl_eval_string("println(20)");
    // std::cout << f(123) <<std::endl;

    std::cout << "<<<<" << std::endl;
}


int main(int argc, char* argv[]) {
    static const int N = 10;
    double a[N];

    std::cout << "---------------------" << std::endl;

    jl_init(); 
    jl_eval_string("println(10)");

    jl_eval_string("f(x::Int)::Int = 2x");
    int (*f)(int) = (int (*)(int))(jl_unbox_voidpointer(jl_eval_string("@cfunction(f, Int, (Int,))")));

    // size_t __excstack_state = jl_excstack_state(); 
    // f(123);

    std::thread t(tf, f);
    t.join();

    jl_eval_string("println(Threads.nthreads())");

    jl_eval_string("a=zeros(4); Threads.@threads for i=1:4; a[i]=Threads.threadid(); end; println(a)"); 

    jl_eval_string("println(11)");
    jl_atexit_hook(0); 

    return 0;
}
