#ifndef FUNC_DATA_HPP__
#define FUNC_DATA_HPP__

#include <vector>
#include <string>
#include <iostream>
#include <unordered_map>

#include "perf_parser.hpp"

namespace HFData {

using callInfo = std::pair<FuncInfo*, std::uint64_t>;

class FuncInfo final {


    std::vector<callInfo> callers_; // another + 0xoffset -> our
    std::vector<callInfo> callees_; // our     + 0xoffset -> another

    int funcSize_ = -1; //!TODO

protected:
    void addCaller (const callInfo& sample) {
        callers_.push_back (sample);
    }

public:

    void addCall (const callInfo& sample) {

        callees_.push_back (sample);
        sample.first->addCaller (sample);

    }

    void setFuncSize (const int funcSize) { funcSize_ = funcSize; }
    int getFuncSize () const { return funcSize_; }

};

class FuncInfoTbl final {

    std::unordered_map<std::string, FuncInfo*> tbl_;

//!TODO

};

}

#endif
