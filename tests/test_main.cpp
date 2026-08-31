#include <iostream>
#include "engine/types.h"
#include "engine/order.h"

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

void test_order_basic() {
    Order o{1001, Side::Sell, 10000, 10, 1};
    CHECK(o.id==1001);
    CHECK(o.side == Side::Sell);
    CHECK(o.px==10000 && o.qty==10);
}

int main() {
    test_sanity();
    test_limits();
    test_order_basic();

    if(fails) { std::cerr<<fails<<" failed\n"; return 1; }
    std::cout<<"all ok\n";
    return 0;
}
