#include <iostream>
#include "engine/types.h"
#include "engine/order.h"
#include "engine/book.h"
#include "engine/engine.h"
#include "engine/parser.h"
#include "engine/format.h"

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

// ---- cancel ----

// T01
void t01_empty() {
    MatchingEngine e;
    CHECK(e.snapshot().empty());
    CHECK(e.cancel(5).err==Err::NOT_FOUND);
}

// T09 - kill head/mid/tail, rest keep order
void t09_cancel_positions() {
    for(int which=0; which<3; which++) {
        MatchingEngine e;
        e.add(1, Side::Sell, 10000, 1);
        e.add(2, Side::Sell, 10000, 1);
        e.add(3, Side::Sell, 10000, 1);
        e.add(4, Side::Sell, 10000, 1);

        OrderId kill = (which==0) ? 1 : (which==1 ? 3 : 4);
        auto c = e.cancel(kill);
        CHECK(c.err==Err::None && c.removed==1);

        auto r = e.add(99, Side::Buy, 10000, 3);
        CHECK(r.trades.size()==3);
        OrderId prev=0;
        for (auto& t : r.trades) {
            CHECK(t.maker != kill);
            CHECK(t.maker > prev);   // still in arrival order
            prev = t.maker;
        }
    }
}

// T10
void t10_cancel_last_at_lvl() {
    MatchingEngine e;
    e.add(1, Side::Buy, 9000, 5);
    e.add(2, Side::Buy, 9100, 5);
    e.cancel(2);
    auto s = e.snapshot();
    CHECK(s.size()==1 && s[0].px==9000);
}

// T11 + example A
void t11_cancel_after_partial() {
    MatchingEngine e;
    e.add(1001, Side::Sell, 10000, 10);
    e.add(1002, Side::Buy, 10000, 6);
    auto s=e.snapshot();
    CHECK(s.size()==1 && s[0].total==4);
    auto c = e.cancel(1001);
    CHECK(c.err==Err::None && c.removed==4);
    CHECK(e.snapshot().empty());
}

// T12 cancelled id cant be reused + T13 repeat cancel
void t12_t13_cancel_ids() {
    MatchingEngine e;
    e.add(5, Side::Buy, 100, 3);
    e.add(6, Side::Buy, 101, 2);
    CHECK(e.cancel(5).removed==3);
    CHECK(e.cancel(5).err == Err::NOT_FOUND);
    CHECK(e.cancel(12345).err == Err::NOT_FOUND);
    CHECK(e.add(5, Side::Buy, 100, 3).err==Err::DUPLICATE_ID);

    auto s = e.snapshot();   // untouched by the failed ones
    CHECK(s.size()==1 && s[0].px==101 && s[0].total==2);
}

// example C follow up
void exC_followup() {
    MatchingEngine e;
    e.add(31, Side::Buy, 10005, 2);
    e.add(32, Side::Buy, 10003, 4);
    e.add(40, Side::Sell, 10004, 5);
    CHECK(e.cancel(31).err == Err::NOT_FOUND);   // filled
    CHECK(e.add(31, Side::Buy, 10000, 1).err == Err::DUPLICATE_ID);
}

// T16 edges should all be accepted
void t16_bounds() {
    MatchingEngine e;
    CHECK(e.add(1, Side::Buy, MIN_PX, minQty).err == Err::None);
    CHECK(e.add(2, Side::Sell, MAX_PX, maxQty).err == Err::None);
    CHECK(e.add(UINT64_MAX, Side::Sell, MAX_PX, maxQty).err==Err::None);
    CHECK(e.cancel(UINT64_MAX).removed == maxQty);

    // big level total, wouldnt fit in 32 bits
    MatchingEngine e2;
    for(OrderId i=1;i<=5;i++) e2.add(i, Side::Buy, 500, maxQty);
    auto s = e2.snapshot();
    CHECK(s.size()==1 && s[0].total == 5*maxQty);

    // max size trade
    auto r = e2.add(10, Side::Sell, 500, maxQty);
    CHECK(r.trades.size()==1 && r.trades[0].qty == maxQty && r.left==0);
}

static bool sameBook(const std::vector<Lvl>& a, const std::vector<Lvl>& b) {
    if(a.size()!=b.size()) return false;
    for(size_t i=0;i<a.size();i++)
        if(a[i].side!=b[i].side || a[i].px!=b[i].px || a[i].total!=b[i].total) return false;
    return true;
}

// T17 rejected stuff leaves everything alone
void t17_atomic() {
    MatchingEngine e;
    e.add(1, Side::Sell, 10000, 5);
    e.add(2, Side::Buy, 9990, 3);
    auto before = e.snapshot();

    // these would all cross if they got through
    CHECK(e.add(1, Side::Buy, 10000, 5).err == Err::DUPLICATE_ID);
    CHECK(e.add(2, Side::Sell, 9990, 3).err == Err::DUPLICATE_ID);
    CHECK(e.add(3, Side::Buy, 10000, 0).err == Err::INVALID_VALUE);
    CHECK(e.add(4, Side::Buy, MAX_PX+1, 5).err == Err::INVALID_VALUE);
    CHECK(e.add(0, Side::Buy, 10000, 5).err == Err::INVALID_VALUE);
    CHECK(e.cancel(77).err == Err::NOT_FOUND);
    CHECK(e.cancel(0).err == Err::INVALID_VALUE);

    CHECK(sameBook(before, e.snapshot()));
    CHECK(e.check()==nullptr);

    // seen ids: 3 + 4 never got reserved, 1 + 2 still are
    CHECK(e.add(3, Side::Buy, 9000, 1).err == Err::None);
    CHECK(e.add(4, Side::Buy, 9000, 1).err == Err::None);
    CHECK(e.add(1, Side::Buy, 9000, 1).err == Err::DUPLICATE_ID);

    // dup + bad value at once -> value wins (precedence)
    CHECK(e.add(2, Side::Buy, 9000, 0).err == Err::INVALID_VALUE);
}

// ---- parser ----

static Err perr(const char* s) { return parse_line(s).err; }

void parse_ok() {
    auto p = parse_line("BUY 1002 10000 6");
    CHECK(p.err==Err::None && p.cmd.type==CmdT::Add && p.cmd.side==Side::Buy);
    CHECK(p.cmd.id==1002 && p.cmd.px==10000 && p.cmd.qty==6);

    p = parse_line("  SELL\t7   9999 1\r");    // weird whitespace + windows line end
    CHECK(p.err==Err::None && p.cmd.side==Side::Sell && p.cmd.qty==1);

    p = parse_line("CANCEL 18446744073709551615");   // uint64 max
    CHECK(p.err==Err::None && p.cmd.type==CmdT::Cancel && p.cmd.id==UINT64_MAX);

    CHECK(parse_line("BOOK").cmd.type == CmdT::Book);
    CHECK(parse_line("").cmd.type == CmdT::Skip);
    CHECK(parse_line("    ").cmd.type==CmdT::Skip);
    CHECK(parse_line("  # hello BUY 1 2 3").cmd.type == CmdT::Skip);
    CHECK(parse_line("BUY 1 1 1").err==Err::None);
    CHECK(parse_line("SELL 1 1000000000 1000000000").err==Err::None);
}

// T14
void t14_bad_numbers() {
    CHECK(perr("BUY 1 0 5") == Err::INVALID_VALUE);
    CHECK(perr("BUY 1 100 0") == Err::INVALID_VALUE);
    CHECK(perr("BUY 0 100 5") == Err::INVALID_VALUE);
    CHECK(perr("BUY 1 -100 5") == Err::INVALID_VALUE);
    CHECK(perr("BUY -1 100 5") == Err::INVALID_VALUE);
    CHECK(perr("SELL 1 1000000001 5") == Err::INVALID_VALUE);
    CHECK(perr("SELL 1 100 1000000001") == Err::INVALID_VALUE);
    CHECK(perr("CANCEL 18446744073709551616") == Err::INVALID_VALUE);   // uint64 max + 1
    CHECK(perr("BUY 1 99999999999999999999999 5") == Err::INVALID_VALUE);
    CHECK(perr("CANCEL 0") == Err::INVALID_VALUE);

    // partial / not numbers
    CHECK(perr("BUY 1 100abc 5") == Err::BAD_COMMAND);
    CHECK(perr("BUY 1 10.5 5") == Err::BAD_COMMAND);
    CHECK(perr("BUY 1 +100 5") == Err::BAD_COMMAND);
    CHECK(perr("BUY 1 - 5") == Err::BAD_COMMAND);
    CHECK(perr("BUY x 100 5") == Err::BAD_COMMAND);
    CHECK(perr("CANCEL 0x10") == Err::BAD_COMMAND);

    // not-a-number beats out of range even if the range one is first
    CHECK(perr("BUY 0 abc 5") == Err::BAD_COMMAND);
}

// T15
void t15_malformed() {
    CHECK(perr("BUY 1 100") == Err::BAD_COMMAND);
    CHECK(perr("BUY 1 100 5 6") == Err::BAD_COMMAND);
    CHECK(perr("CANCEL") == Err::BAD_COMMAND);
    CHECK(perr("CANCEL 1 2") == Err::BAD_COMMAND);
    CHECK(perr("BOOK now") == Err::BAD_COMMAND);
    CHECK(perr("buy 1 100 5") == Err::BAD_COMMAND);   // case sensitive
    CHECK(perr("AMEND 1 100 5") == Err::BAD_COMMAND);
    CHECK(perr("BUY 1 100 5 # inline comment") == Err::BAD_COMMAND);
}

// ---- formatter ----

// example A, exact text
void fmt_exA() {
    MatchingEngine e;
    CHECK(fmt_add(1001, e.add(1001, Side::Sell, 10000, 10)) == "ACK 1001\nREST 1001 10\n");
    CHECK(fmt_add(1002, e.add(1002, Side::Buy, 10000, 6)) ==
          "ACK 1002\nTRADE 1001 1002 10000 6\nDONE 1002\n");
    CHECK(fmt_book(e.snapshot()) == "BOOK_BEGIN\nASK 10000 4\nBOOK_END\n");
    CHECK(fmtCancel(1001, e.cancel(1001)) == "CANCELLED 1001 4\n");
    CHECK(fmt_book(e.snapshot()) == "BOOK_BEGIN\nBOOK_END\n");
}

void fmt_errors_and_book() {
    MatchingEngine e;
    CHECK(fmtCancel(9, e.cancel(9)) == "ERROR NOT_FOUND\n");
    e.add(1, Side::Buy, 100, 1);
    CHECK(fmt_add(1, e.add(1, Side::Buy, 100, 1)) == "ERROR DUPLICATE_ID\n");
    CHECK(fmt_add(2, e.add(2, Side::Buy, 100, 0)) == "ERROR INVALID_VALUE\n");
    CHECK(fmt_err(Err::BAD_COMMAND) == "ERROR BAD_COMMAND\n");

    e.add(2, Side::Buy, 101, 2);
    e.add(3, Side::Sell, 105, 3);
    e.add(4, Side::Sell, 103, 4);
    CHECK(fmt_book(e.snapshot()) ==
          "BOOK_BEGIN\nBID 101 2\nBID 100 1\nASK 103 4\nASK 105 3\nBOOK_END\n");
}

// tiny lcg, so the sequence is the same everywhere (std distributions arent)
struct Rng {
    std::uint64_t s;
    std::uint64_t next() { s = s*6364136223846793005ULL + 1442695040888963407ULL; return s>>33; }
    std::int64_t in(std::int64_t lo, std::int64_t hi) { return lo + (std::int64_t)(next() % (std::uint64_t)(hi-lo+1)); }
};

static Qty resting_total(const MatchingEngine& e) {
    Qty t=0;
    for (auto &l : e.snapshot()) t+=l.total;
    return t;
}

void invariants_random() {
    MatchingEngine e;
    Rng r{42};
    OrderId nid=1;
    int bad=0;
    Qty accepted=0, cancelled=0, traded=0;
    int rejects=0;

    for(int i=0;i<2000;i++) {
        int op = (int)r.in(0, 9);
        if(op<7) {
            Side sd = r.in(0,1) ? Side::Buy : Side::Sell;
            Qty q = r.in(0, 20);   // 0 sometimes on purpose -> rejected
            OrderId id = (op==6 && nid>1) ? (OrderId)r.in(1, (std::int64_t)nid-1) : nid++;
            // ^ op 6 reuses an old id, should always bounce
            auto res = e.add(id, sd, r.in(9990, 10010), q);
            if(res.err==Err::None) {
                accepted += q;
                for(auto &t : res.trades) traded += t.qty;
            } else rejects++;
        } else {
            auto c = e.cancel((OrderId)r.in(1, (std::int64_t)nid));
            cancelled += c.removed;
        }

        const char* why = e.check();
        if(why) { std::cerr<<"step "<<i<<": "<<why<<"\n"; bad++; break; }

        // every trade eats qty from both sides, hence 2x
        if(accepted != resting_total(e) + cancelled + 2*traded) {
            std::cerr<<"step "<<i<<": qty not conserved\n"; bad++; break;
        }
    }
    CHECK(bad==0);
    CHECK(traded > 0 && cancelled > 0 && rejects > 0);   // make sure the mix actually did stuff
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

    t01_empty();
    t09_cancel_positions();
    t10_cancel_last_at_lvl();
    t11_cancel_after_partial();
    t12_t13_cancel_ids();
    exC_followup();
    t16_bounds();
    t17_atomic();
    parse_ok();
    t14_bad_numbers();
    t15_malformed();
    fmt_exA();
    fmt_errors_and_book();
    invariants_random();

    if(fails) { std::cerr<<fails<<" failed\n"; return 1; }
    std::cout<<"all ok\n";
    return 0;
}
