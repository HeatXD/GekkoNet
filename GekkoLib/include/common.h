#pragma once

#include "gekko_types.h"

#include <random>


namespace Gekko {
    struct Common {
        static u32 RNG() {
            std::random_device seed;
            std::mt19937 gen(seed());
            std::uniform_int_distribution<u32> dis(1, 100000);
            return dis(gen);
        }
    };
}
