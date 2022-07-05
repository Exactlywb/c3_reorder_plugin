#ifndef FUNC_DATA_HPP__
#define FUNC_DATA_HPP__

#include <vector>
#include <string>
#include <iostream>
#include <unordered_map>
#include <set>

#include "perf_parser.hpp"

namespace HFData {

namespace {

struct pairHash final {

    template<class T1, class T2>
    std::size_t operator() (const std::pair<T1, T2> &pair) const {
        return std::hash<T1>() (pair.first) ^ std::hash<T2>() (pair.second);
    }

};

}

class FuncInfo final {

    using callInfo  = std::pair<std::size_t, std::uint64_t>;
    using calls     = std::vector<callInfo>; //!TODO change it
    std::unordered_map<FuncInfo*, calls> callees_;  // our + 0xoffset -> another
                                                    // (std::size_t is occurance)

    std::string funcName_;

    int funcSize_ = -1; //!TODO

public:

    FuncInfo (const std::string& funcName): funcName_ (funcName) {}

    void addCall (FuncInfo* callee, const std::uint64_t offset) {

        auto it = callees_.find (callee);
        if (it != callees_.end ()) {
            
            for (auto el: (*it).second) {

                if (el.second == offset) {

                    el.first++;
                    return;

                }

            }

            (*it).second.push_back ({1, offset});

        }

        callees_.insert ({callee, {{1, offset}}});

    }

    std::string getFuncName () const { return funcName_; }

    void setFuncSize (const int funcSize) { funcSize_ = funcSize; }
    int getFuncSize () const { return funcSize_; }

};

namespace {

    void FillTbl (std::unordered_map<std::string, FuncInfo*>& tbl, 
                  const std::vector<perfParser::LbrSample>& samples)
    {

        std::set<std::string> funcNames; //!TODO static functions in different files
        for (auto sample: samples) {

            funcNames.insert (sample.calleeName_);
            funcNames.insert (sample.callerName_);

        }

        for (auto name: funcNames)
            tbl.insert ({name, new FuncInfo (name)});

    }

}

class FuncInfoTbl final {

    std::unordered_map<std::string, FuncInfo*> tbl_;

public:
    FuncInfoTbl (const std::vector<perfParser::LbrSample>& samples) {

        FillTbl (tbl_, samples);
        
    }

    using tblIt = std::unordered_map<std::string, FuncInfo*>::const_iterator;

    tblIt lookup (const std::string& str) const 
    {
        return tbl_.find (str);
    }

    tblIt begin () const { return tbl_.begin (); }
    tblIt end () const { return tbl_.end (); }

    std::size_t size () const { return tbl_.size (); }

};

}

#endif
