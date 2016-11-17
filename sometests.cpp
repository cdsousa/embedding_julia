#include <iostream>
#include <thread>
#include <chrono>

#include <julia.h>


void tf(int (*f)(int)){
    std::cout << ">>>>" << std::endl;
//     jl_eval_string("println(20)");
    std::cout << f(123) <<std::endl;
    std::cout << "<<<<" << std::endl;
}


int main(int argc, char* argv[]) {
    static const int N = 10;
    double a[N];

    jl_init(JULIA_INIT_DIR);
    jl_eval_string("println(10)");

    jl_eval_string("f(x::Int)::Int = 2x");
    int (*f)(int) = (int (*)(int))(jl_unbox_voidpointer(jl_eval_string("cfunction(f, Int, (Int,))")));

    std::thread t(tf, f);
    t.join();



    jl_eval_string("println(11)");
    jl_atexit_hook(0);

    return 0;
}
