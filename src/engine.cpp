#include "engine/engine.h"

namespace eng {

AddRes MatchingEngine::add(OrderId id, Side s, Px px, Qty q)
{
    AddRes r;
    // domain first, then dup. order matters (pdd s3)
    if(id==0 || px<MIN_PX || px>MAX_PX || q<minQty || q>maxQty) {
        r.err = Err::INVALID_VALUE;
        return r;
    }
    if(seen.count(id)) { r.err = Err::DUPLICATE_ID; return r; }

    // nothing above touched state. from here we're committed
    seen.insert(id);
    Order o{id, s, px, q, nxtSeq++};
    r.trades = book.match(o);

    Qty filled=0;
    for (auto &t : r.trades) filled += t.qty;
    r.left = q - filled;
    return r;
}

CxlRes MatchingEngine::cancel(OrderId id)
{
    CxlRes r;
    if(id == 0) { r.err = Err::INVALID_VALUE; return r; }
    //if(!seen.count(id)) { r.err = Err::NOT_FOUND; return r; }
    // ^ pointless, the book scan already covers it

    r.removed = book.cancel(id);
    if(r.removed == 0) r.err = Err::NOT_FOUND;
    // id stays in seen, cancelled ids cant come back
    return r;
}

void MatchingEngine::reset() {
    book = OrderBook();
    seen.clear();
    nxtSeq = 1;
}

}
