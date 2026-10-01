#pragma once
#include <string>
#include "engine/engine.h"

namespace eng {

enum class CmdT { Add, Cancel, Book, Skip };   // Skip = blank line or # comment

struct Cmd {
    CmdT type = CmdT::Skip;
    Side side = Side::Buy;
    OrderId id = 0;
    Px px = 0;
    Qty qty = 0;
};

struct ParseRes {
    Err err = Err::None;
    Cmd cmd;
};

ParseRes parse_line(const std::string& line);

}
