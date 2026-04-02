#include "helper.h"
#include <iostream>

extern "C" void print_result(int n) {
    std::cout << "Result: " << n << std::endl;
}
