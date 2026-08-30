#include <iostream>

static int fails=0;

#define CHECK(x) do { if(!(x)) { std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<"  "<<#x<<"\n"; fails++; } } while(0)
// ^ keeps going after a failure so we see all of them in one run

void test_sanity()
{
    CHECK(1+1==2);
}

int main() {
    test_sanity();

    if(fails) { std::cerr<<fails<<" failed\n"; return 1; }
    std::cout<<"all ok\n";
    return 0;
}
