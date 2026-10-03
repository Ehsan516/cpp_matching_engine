#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include "engine/parser.h"
#include "engine/format.h"

using namespace eng;

// returns exit code. 0 all good, 1 something got rejected
static int run(std::istream &in)
{
    MatchingEngine e;
    std::string line;
    long ln = 0;
    bool anyBad=false;

    while(std::getline(in, line)) {
        ln++;
        ParseRes p = parse_line(line);
        std::string out;
        Err err = p.err;

        if(err == Err::None) {
            switch(p.cmd.type) {
            case CmdT::Skip:
                continue;
            case CmdT::Book:
                out = fmt_book(e.snapshot());
                break;
            case CmdT::Add: {
                AddRes r = e.add(p.cmd.id, p.cmd.side, p.cmd.px, p.cmd.qty);
                err = r.err;
                out = fmt_add(p.cmd.id, r);
                break;
            }
            case CmdT::Cancel: {
                CxlRes c = e.cancel(p.cmd.id);
                err=c.err;
                out = fmtCancel(p.cmd.id, c);
                break;
            }
            }
        } else {
            out = fmt_err(err);
        }

        std::cout << out;
        if(err != Err::None) {
            anyBad = true;
            std::cerr << "line " << ln << ": " << out;   // out already has the \n
        }
    }
    if(in.bad()) { std::cerr<<"read error\n"; return 2; }
    return anyBad ? 1 : 0;
}

int main(int argc, char** argv) {
    //std::cout<<"matcher v0\n";
    std::ios::sync_with_stdio(false);

    if(argc==1) return run(std::cin);

    if(argc==3 && std::strcmp(argv[1], "--input")==0) {
        std::ifstream f(argv[2]);
        if(!f) {
            std::cerr << "cant open " << argv[2] << "\n";
            return 2;
        }
        return run(f);
    }

    std::cerr<<"usage: matcher [--input file]\n";
  return 2;
}
