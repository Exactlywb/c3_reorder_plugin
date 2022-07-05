#include "../include/funcData.hpp"

namespace HFData {

    void FuncInfo::addCall (FuncInfo *callee,
                            const std::uint64_t offset)
    {
        auto it = callees_.find (callee);
        if (it != callees_.end ()) {
            for (auto el : (*it).second) {
                if (el.second == offset) {
                    el.first++;
                    return;
                }
            }

            (*it).second.push_back ({1, offset});
        }

        callees_.insert ({callee, {{1, offset}}});
    }

    FuncInfoTbl::FuncInfoTbl (
        const std::vector<perfParser::LbrSample> &samples)
    {
        std::set<std::string>
            funcNames;  //! TODO static functions in different files
        for (auto sample : samples) {
            funcNames.insert (sample.calleeName_);
            funcNames.insert (sample.callerName_);
        }

        for (auto name : funcNames)
            tbl_.insert ({name, new FuncInfo (name)});
    }

}  // namespace HFData
