#include "engine/format.h"

namespace eng {

static const char* errName(Err e) {
    switch(e) {
        case Err::DUPLICATE_ID: return "DUPLICATE_ID";
        case Err::INVALID_VALUE: return "INVALID_VALUE";
        case Err::NOT_FOUND: return "NOT_FOUND";
        case Err::BAD_COMMAND: return "BAD_COMMAND";
        case Err::None: break;
    }
    return "???";
}

std::string fmt_err(Err e) {
    return std::string("ERROR ") + errName(e) + "\n";
}

std::string fmt_add(OrderId id, const AddRes &r)
{
    if(r.err!=Err::None) return fmt_err(r.err);

    std::string s = "ACK " + std::to_string(id) + "\n";
    for (auto& t : r.trades) {
        s += "TRADE " + std::to_string(t.maker) + " " + std::to_string(t.taker) + " "
           + std::to_string(t.px) + " " + std::to_string(t.qty) + "\n";
    }
    if(r.left>0) s += "REST " + std::to_string(id) + " " + std::to_string(r.left) + "\n";
    else         s += "DONE " + std::to_string(id) + "\n";
    // no DONE for makers, only the incoming one
    return s;
}

std::string fmtCancel(OrderId id, const CxlRes &r)
{
    if(r.err != Err::None) return fmt_err(r.err);
    return "CANCELLED " + std::to_string(id) + " " + std::to_string(r.removed) + "\n";
}

std::string fmt_book(const std::vector<Lvl> &lv) {
    std::string s="BOOK_BEGIN\n";
    for(auto &l : lv) {
        s += (l.side==Side::Buy ? "BID " : "ASK ");
        s += std::to_string(l.px) + " " + std::to_string(l.total) + "\n";
    }
    //  ^ relies on snapshot() already being bids desc then asks asc
    s += "BOOK_END\n";
    return s;
}

}
