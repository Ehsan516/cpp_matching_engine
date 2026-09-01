#include "engine/book.h"

namespace eng {

void OrderBook::add_resting(const Order &o)
{
    if(o.side == Side::Sell) {
        asks[o.px].push_back(o);
    }
    // TODO bids
}

}
