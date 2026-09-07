#include <iostream>
#include "engine/types.h"
#include "engine/order.h"
#include "engine/book.h"
#include "engine/engine.h"

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

// T02
void t02_no_cross() {
    OrderBook b;
    auto t1 = b.match({1, Side::Buy, 9999, 5, 1});
    auto t2 = b.match({2,Side::Sell,10000,5,2});
    CHECK(t1.empty() && t2.empty());
    CHECK(b.bidLevels()==1 && b.askLevels()==1);
}

// T03
void t03_full_match() {
    OrderBook b;
    b.match({1, Side::Sell, 10000, 7, 1});
    auto tr = b.match({2, Side::Buy, 10000, 7, 2});
    CHECK(tr.size()==1);
    CHECK(tr.size()==1 && tr[0].qty==7 && tr[0].px==10000);
    CHECK(b.snapshot().empty());   // both gone
}

// T04 incoming smaller, maker keeps rest + its place in line
void t04_maker_keeps_spot() {
    OrderBook b;
    b.match({1, Side::Sell, 10000, 10, 1});
    b.match({2, Side::Sell, 10000, 5, 2});

    auto a = b.match({3, Side::Buy, 10000, 4, 3});
    CHECK(a.size()==1 && a[0].maker==1 && a[0].qty==4);

    auto c = b.match({4, Side::Buy, 10000, 7, 4});
    CHECK(c.size()==2);
    if(c.size()<2) return;
    CHECK(c[0].maker==1 && c[0].qty==6);   // 1 still first
    CHECK(c[1].maker==2 && c[1].qty==1);
}

//T05
void t05_incoming_bigger() {
    OrderBook b;
    b.match({1,Side::Buy,10000,3,1});
    auto tr = b.match({2, Side::Sell, 9995, 8, 2});
    CHECK(tr.size()==1 && tr[0].qty==3);
    auto s = b.snapshot();
    CHECK(s.size()==1);
    if(s.empty()) return;
    CHECK(s[0].side==Side::Sell && s[0].px==9995 && s[0].total==5);
}

// T07 sweep (ex B) + stops at limit
void t07_sweep() {
    OrderBook b;
    b.match({11, Side::Sell, 10000, 4, 1});
    b.match({12, Side::Sell, 10000, 5, 2});
    b.match({13, Side::Sell, 10002, 3, 3});
    b.match({14, Side::Sell, 10003, 9, 4});   // above limit, shouldnt touch

    auto tr = b.match({20, Side::Buy, 10002, 10, 5});
    CHECK(tr.size()==3);
    if(tr.size()!=3) return;
    CHECK(tr[0].maker==11 && tr[0].px==10000 && tr[0].qty==4);
    CHECK(tr[1].maker==12 && tr[1].px==10000 && tr[1].qty==5);
    CHECK(tr[2].maker==13 && tr[2].px==10002 && tr[2].qty==1);

    auto s=b.snapshot();
    CHECK(s.size()==2);
    if(s.size()!=2) return;
    CHECK(s[0].px==10002 && s[0].total==2);
    CHECK(s[1].px==10003 && s[1].total==9);
}

// T08 buy side. sell side is in test_exC
void t08_px_improve_buy() {
    OrderBook b;
    b.match({1, Side::Sell, 9990, 2, 1});
    auto tr = b.match({2, Side::Buy, 10000, 2, 2});
    CHECK(tr.size()==1 && tr[0].px==9990);
}

// ---- engine level ----

void eng_add_basic() {
    MatchingEngine e;
    auto r = e.add(1001, Side::Sell, 10000, 10);
    CHECK(r.err==Err::None && r.trades.empty() && r.left==10);
    r = e.add(1002, Side::Buy, 10000, 6);
    CHECK(r.err==Err::None && r.trades.size()==1 && r.left==0);
}

void eng_dup_ids() {
    MatchingEngine e;
    e.add(1, Side::Sell, 10000, 5);
    CHECK(e.add(1, Side::Buy, 9000, 1).err == Err::DUPLICATE_ID);   // active

    e.add(2, Side::Buy, 10000, 5);   // fills 1 and 2 fully
    CHECK(e.add(2, Side::Buy, 9000, 1).err == Err::DUPLICATE_ID);   // filled
    CHECK(e.add(1, Side::Sell, 9000, 1).err==Err::DUPLICATE_ID);
    CHECK(e.snapshot().empty());
}

void eng_invalid() {
    MatchingEngine e;
    CHECK(e.add(0, Side::Buy, 100, 1).err == Err::INVALID_VALUE);
    CHECK(e.add(1, Side::Buy, 0, 1).err == Err::INVALID_VALUE);
    CHECK(e.add(1, Side::Buy, 100, 0).err == Err::INVALID_VALUE);
    CHECK(e.add(1, Side::Buy, -5, 1).err == Err::INVALID_VALUE);
    CHECK(e.add(1, Side::Buy, MAX_PX+1, 1).err == Err::INVALID_VALUE);
    CHECK(e.add(1, Side::Buy, 100, maxQty+1).err == Err::INVALID_VALUE);

    // rejected adds dont reserve the id
    CHECK(e.add(1, Side::Buy, 100, 1).err == Err::None);
    CHECK(e.snapshot().size()==1);
}

void eng_reset() {
    MatchingEngine e;
    e.add(7, Side::Buy, 100, 1);
    e.reset();
    CHECK(e.snapshot().empty());
    CHECK(e.add(7, Side::Buy, 100, 1).err == Err::None);
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
    t02_no_cross();
    t03_full_match();
    t04_maker_keeps_spot();
    t05_incoming_bigger();
    t07_sweep();
    t08_px_improve_buy();

    eng_add_basic();
    eng_dup_ids();
    eng_invalid();
    eng_reset();

    if(fails) { std::cerr<<fails<<" failed\n"; return 1; }
    std::cout<<"all ok\n";
    return 0;
}
