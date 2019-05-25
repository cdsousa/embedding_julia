#include <iostream>
#include <thread>
#include <chrono>
#include <future>



#define JULIA_ENABLE_THREADING
#include <platform.h>
#include <julia.h>

void f(std::string* x){
    std::cout << "!!!!!!!!!!!  " << *static_cast<std::string*>(x) << std::endl;
}


int main(int argc, char* argv[]) {

    std::cout << "---------------------" << std::endl;

    jl_init(); 
    jl_eval_string("println(\">>> julia in <<<\")");


//////////////////////////


jl_function_t *func = jl_get_function(jl_base_module, "sqrt");
jl_value_t *argument = jl_box_float64(2.0);
jl_value_t *ret = jl_call1(func, argument);


//////////////////////////



    std::string aaa = "asdasdasdasd";


    std::string je = "ccall(Ptr{Cvoid}(" + 
        std::to_string(reinterpret_cast<std::size_t>(&f)) +
        "), Cvoid, (Ptr{Cvoid},), Ptr{Cvoid}(" +
        std::to_string(reinterpret_cast<std::size_t>(&aaa)) +
        "))";

    std::cout << je << std::endl;

    jl_eval_string(je.c_str());

    if (jl_exception_occurred()){
        std::cout << jl_typeof_str(jl_exception_occurred()) << std::endl;
    }




//////////////////////////


    jl_eval_string("println(\">>> julia out <<<\")");
    jl_atexit_hook(0); 

    std::cout << "---------------------" << std::endl;

    return 0;
}
