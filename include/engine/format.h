#pragma once
#include <string>
#include "engine/engine.h"

namespace eng {

// everything returns full lines incl the trailing \n

std::string fmt_err(Err e);
std::string fmt_add(OrderId id, const AddRes &r);
std::string fmtCancel(OrderId id, const CxlRes &r);
std::string fmt_book(const std::vector<Lvl> &lv);

}
