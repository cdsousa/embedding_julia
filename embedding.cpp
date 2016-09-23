#include <iostream>

#include <julia.h>

int main(int argc, char* argv[]) {
    static const int N = 10;
    double a[N];

    /* required: setup the Julia context */
    jl_init(JULIA_INIT_DIR);


    //////////// Usual stuff

    /* run Julia commands */
    jl_eval_string("println(sqrt(2.0))");

    jl_value_t* array_type = jl_apply_array_type(jl_float64_type, 1);
    jl_array_t* a_jl = jl_ptr_to_array_1d(array_type, a, N, 0);

    jl_eval_string("function g(a) \n a[:] = 1:length(a) \nend");
    jl_call1(jl_get_function(jl_main_module, "g"), (jl_value_t*)a_jl);

    jl_eval_string("f(x) = x .^ 2");
    jl_function_t* func = jl_get_function(jl_main_module, "f");
    for (int i = 0; i < N; ++i) {
        double ret = jl_unbox_float64(jl_call1(func, jl_box_float64(a[i])));
        std::cout << ret << std::endl;
    }



    std::cout << std::endl;


    //////////// Getting a native function pointer to a Julia function

    jl_eval_string("f(x::Int)::Int = 2x");

    int (*f)(int) = (int (*)(int))(jl_unbox_voidpointer(jl_eval_string("cfunction(f, Int, (Int,))")));

    std::cout << "This is really nice! " <<  f(123) << std::endl;

    //     jl_eval_string("@code_native f(123)");



    //     jl_eval_string("this_function_does_not_exist()");
    if (jl_exception_occurred()) {
        jl_show(jl_stderr_obj(), jl_exception_occurred());
        jl_printf(jl_stderr_stream(), "\n");
    }


    /* strongly recommended: notify Julia that the
         program is about to terminate. this allows
         Julia time to cleanup pending write requests
         and run all finalizers
    */
    jl_atexit_hook(0);

    std::cout << " ... Ok ... " << std::endl;

    return 0;
}
