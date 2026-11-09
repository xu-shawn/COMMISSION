// fractional vertical offset (signed, f - 0.5) of octaves 0A 0B 1A 1B 2A 2B for each "seed ..." line on stdin
#include "cpipe.hpp"
#include <cstdio>
#include <iostream>
#include <sstream>
using namespace cp;
int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream is(line); long long sd; if (!(is >> sd)) continue;
        std::string rest; std::getline(is, rest);
        Seed s{uint64_t(sd)};
        printf("%lld", sd);
        for (int o = 0; o < 5; o++) for (int side = 0; side < 2; side++) {
            const auto &salt = standalone::octave_salt[-9 + o + 10];
            Random r(s.fa[side] ^ salt[0], s.fb[side] ^ salt[1]); r.next();
            float f = float(uint32_t((r.next() >> 32) & 0xFFFFFF)) * 5.9604645E-8f;
            printf(" %.5f", f - 0.5f);
        }
        printf("%s\n", rest.c_str());
    }
}
