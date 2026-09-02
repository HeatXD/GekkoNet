#include "debug.h"

#include <iostream>
#include <cstdlib>

namespace Gekko {
    void Assert(bool condition, std::string_view message, std::source_location location)
    {
        if (!condition) 
        {
            std::cerr
                << "Assertion Failed!\n"
                << "file: " << location.file_name() 
                << ":" << location.line()
                << " " << location.function_name();
    
            if (!message.empty()) { std::cerr << ": " << message; }
            
            std::cerr << '\n';
            
            std::abort();
        }
    }
}
