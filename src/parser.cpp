#include "engine/parser.h"
#include <sstream>
#include <vector>
#include <charconv>

namespace eng {

enum NumR { N_OK, N_NOTNUM, N_RANGE };

// only accepts -?[0-9]+ . anything else ("12abc", "1.5", "+3", "") is not a number
// negatives are valid syntax but every field we have is >=1 so they're just out of range
static NumR rd(const std::string &s, std::uint64_t &out)
{
    size_t i = 0;
    bool neg = false;
    if(!s.empty() && s[0]=='-') { neg=true; i=1; }
    if(i>=s.size()) return N_NOTNUM;
    for(size_t j=i;j<s.size();j++)
        if(s[j]<'0' || s[j]>'9') return N_NOTNUM;
    if(neg) return N_RANGE;

    auto res = std::from_chars(s.data(), s.data()+s.size(), out);
    if(res.ec == std::errc::result_out_of_range) return N_RANGE;
    // if(res.ec != std::errc()) return N_NOTNUM;   cant happen, digits checked above
    return N_OK;
}

ParseRes parse_line(const std::string& line)
{
    ParseRes r;

    size_t first = line.find_first_not_of(" \t\r\n");
    if(first == std::string::npos || line[first]=='#') return r;   // skip

    std::vector<std::string> tk;
    std::istringstream ss(line);
    std::string w;
    while(ss >> w) tk.push_back(w);

    const std::string &verb = tk[0];
    size_t want;
    if(verb=="BUY" || verb=="SELL") want = 4;
    else if(verb=="CANCEL") want = 2;
    else if(verb=="BOOK") want = 1;
    else { r.err = Err::BAD_COMMAND; return r; }

    if(tk.size()!=want) { r.err=Err::BAD_COMMAND; return r; }

    if(verb=="BOOK") { r.cmd.type = CmdT::Book; return r; }

    // pass 1: is everything a number at all. BAD_COMMAND beats INVALID_VALUE
    std::uint64_t v[3] = {0,0,0};
    bool outOfRange = false;
    for(size_t i=1;i<tk.size();i++) {
        NumR nr = rd(tk[i], v[i-1]);
        if(nr==N_NOTNUM) { r.err = Err::BAD_COMMAND; return r; }
        if(nr==N_RANGE) outOfRange = true;
    }
    if(outOfRange) { r.err = Err::INVALID_VALUE; return r; }

    // pass 2: domain
    if(v[0]==0) { r.err = Err::INVALID_VALUE; return r; }
    r.cmd.id = v[0];

    if(verb=="CANCEL") {
        r.cmd.type = CmdT::Cancel;
        return r;
    }

    if(v[1] < (std::uint64_t)MIN_PX || v[1] > (std::uint64_t)MAX_PX ||
       v[2] < (std::uint64_t)minQty || v[2] > (std::uint64_t)maxQty) {
        r.err=Err::INVALID_VALUE; return r;
    }
    r.cmd.type = CmdT::Add;
    r.cmd.side = (verb=="BUY") ? Side::Buy : Side::Sell;
    r.cmd.px = (Px)v[1];
    r.cmd.qty = (Qty)v[2];
    return r;
}

}
