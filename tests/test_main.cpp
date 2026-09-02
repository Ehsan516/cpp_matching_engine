#include <iostream>
#include "engine/types.h"
#include "engine/order.h"
#include "engine/book.h"

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

void test_two_sells() {
    OrderBook b;
    b.add_resting({1, Side::Sell, 10002, 5, 1});
    b.add_resting({2, Side::Sell, 10000, 3, 2});
    CHECK(b.askLevels()==2);
    CHECK(b.bestAsk()==10000);

    b.add_resting({3,Side::Sell,10000,1,3});
    CHECK(b.askLevels()==2);   // same level, not a new one
}

void test_bids_sorted(){
    OrderBook b;
    b.add_resting({5, Side::Buy, 9990, 1, 1});
    b.add_resting({6, Side::Buy, 9995, 2, 2});
    b.add_resting({7, Side::Buy, 9980, 4, 3});
    CHECK(b.bidLevels()==3);
    CHECK(b.bestBid() == 9995);
    CHECK(b.askLevels()==0);
}

int main() {
    test_sanity();
    test_limits();
    test_order_basic();
    test_two_sells();
    test_bids_sorted();

    if(fails) { std::cerr<<fails<<" failed\n"; return 1; }
    std::cout<<"all ok\n";
    return 0;
}
