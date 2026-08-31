#include <iostream>
#include "engine/types.h"

using namespace eng;

static int fails=0;

#define CHECK(x) do { if(!(x)) { std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<"  "<<#x<<"\n"; fails++; } } while(0)
// ^ keeps going after a failure so we see all of them in one run

void test_sanity()
{
    CHECK(1+1==2);
}

void test_limits(){
    CHECK(MIN_PX<MAX_PX);
    CHECK(maxQty == 1000000000);
    CHECK(Side::Buy!=Side::Sell);
}

int main() {
    test_sanity();
    test_limits();

    if(fails) { std::cerr<<fails<<" failed\n"; return 1; }
    std::cout<<"all ok\n";
    return 0;
}
