#pragma once
#include <cstdint>

namespace eng {

using OrderId = std::uint64_t;
using Px  = std::int64_t;   // ticks, 1 tick = 0.01
using Qty = std::int64_t;
// using Price = double;   no. integer ticks only

enum class Side { Buy, Sell };

const Px MIN_PX = 1;
const Px MAX_PX = 1000000000;
const Qty minQty = 1;
const Qty maxQty = 1000000000;
// ^ both limits inclusive, see PDD section 2

}
