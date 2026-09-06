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

void test_snapshot() {
    OrderBook b;
    CHECK(b.snapshot().empty());

    b.add_resting({1, Side::Sell, 10002, 3, 1});
    b.add_resting({2, Side::Buy, 9998, 4, 2});
    b.add_resting({3, Side::Sell, 10000, 5, 3});
    b.add_resting({4, Side::Sell, 10000, 2, 4});
    b.add_resting({5, Side::Buy, 9999, 1, 5});

    auto s = b.snapshot();
    CHECK(s.size()==4);
    if(s.size()!=4) return;
    CHECK(s[0].side==Side::Buy && s[0].px==9999 && s[0].total==1);
    CHECK(s[1].side==Side::Buy && s[1].px==9998 && s[1].total==4);
    CHECK(s[2].side==Side::Sell&& s[2].px==10000&& s[2].total==7);
    CHECK(s[3].side==Side::Sell && s[3].px==10002 && s[3].total==3);
}

// T06 - same price queue, oldest first
void test_fifo_same_px() {
    OrderBook b;
    b.match({11, Side::Sell, 10000, 4, 1});
    b.match({12, Side::Sell, 10000, 5, 2});
    auto tr = b.match({20, Side::Buy, 10000, 6, 3});

    CHECK(tr.size()==2);
    if (tr.size()<2) return;
    CHECK(tr[0].maker==11 && tr[0].taker==20 && tr[0].qty==4);
    CHECK(tr[1].maker==12 && tr[1].qty==2);
}

void test_fifo_sell_side() {
    OrderBook b;
    b.match({31, Side::Buy, 10000, 4, 1});
    b.match({32, Side::Buy, 10000, 5, 2});
    auto tr = b.match({40, Side::Sell, 10000, 6, 3});
    CHECK(tr.size()==2);
    if(tr.size()<2) return;
    CHECK(tr[0].maker==31 && tr[0].taker==40 && tr[0].qty==4);
    CHECK(tr[1].maker==32 && tr[1].qty==2);
    CHECK(b.bidLevels()==1);
}

// example C from the pdd
void test_exC(){
    OrderBook b;
    b.match({31, Side::Buy, 10005, 2, 1});
    b.match({32, Side::Buy, 10003, 4, 2});
    auto tr = b.match({40, Side::Sell, 10004, 5, 3});

    CHECK(tr.size()==1);
    if (tr.empty()) return;
    CHECK(tr[0].maker==31 && tr[0].px==10005 && tr[0].qty==2);  // resting px not 10004

    auto s = b.snapshot();
    CHECK(s.size()==2);
    if(s.size()!=2) return;
    CHECK(s[0].side==Side::Buy && s[0].px==10003 && s[0].total==4);
    CHECK(s[1].side==Side::Sell && s[1].px==10004 && s[1].total==3);
}

int main() {
    test_sanity();
    test_limits();
    test_order_basic();
    test_two_sells();
    test_bids_sorted();
    test_snapshot();
    test_fifo_same_px();
    test_fifo_sell_side();
    test_exC();

    if(fails) { std::cerr<<fails<<" failed\n"; return 1; }
    std::cout<<"all ok\n";
    return 0;
}
