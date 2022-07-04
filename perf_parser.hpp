#ifndef PERF_PARSER_HPP__
#define PERF_PARSER_HPP__

#include <iostream>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/convert.hpp>
#include <unordered_map>
#include <iterator>

namespace perfParser {

class TraceStream final {
    std::string currentLine_;
    std::ifstream perfFile_;

    bool isAtEOF_ = false;
    uint64_t lineNumber_ = 0;

public:
    TraceStream (const std::string& fileName):
        perfFile_ (fileName)
    {

        if (!perfFile_.is_open ())
            throw std::runtime_error ("Can't open perf script file");

        advance ();

    }

    bool isAtEOF () const { return isAtEOF_; }
    std::string getCurrentLine () const {
        
        if (isAtEOF_)
            throw std::runtime_error ("Line iterator reaches the End-of-File!");
        return currentLine_;
    
    }

    uint64_t getLineNumber () const { return lineNumber_; }

    void advance () {

        std::string curLine;
        if (!std::getline (perfFile_, curLine)) {
            isAtEOF_ = true;
            return;
        }
        
        currentLine_ = curLine;
        lineNumber_++;

    }

    ~TraceStream () { perfFile_.close (); }

};

enum PerfContent {

    Unknown,
    LBR,
    LBRStack

};

// A LBR sample is like:
// 40062f 0x5c6313f/0x5c63170/P/-/-/0  0x5c630e7/0x5c63130/P/-/-/0 ...
// A heuristic for fast detection by checking whether a
// leading "0x" and the '/' exist.
bool isLBRSample (const std::string& str) {

    std::vector<std::string> record;
    
    boost::split (record, boost::trim_copy (str), boost::is_any_of (" "));

    if (record.size () < 2)
        return false;
    if (boost::contains (record [1], "0x") == 0
        && boost::contains (record [1], "/")  == 0)
        return true;

    return false;

}

PerfContent checkPerfScriptType (const char* perf_script_path) {

    perfParser::TraceStream traceReader (perf_script_path);

    while (!traceReader.isAtEOF ()) {

        while (!traceReader.getCurrentLine ().size ())
            traceReader.advance ();
            
        // Check for call stack
        int count = 0;
        int addr = 0;
        while (!traceReader.isAtEOF ()
               && boost::conversion::try_lexical_convert<int> (boost::trim_left_copy (traceReader.getCurrentLine ()), addr)) {
            count++;
            traceReader.advance ();
        }

        if (!traceReader.isAtEOF ()) {

            if (isLBRSample (traceReader.getCurrentLine ())) {
                if (count > 0)
                    return PerfContent::LBRStack;
                else
                    return PerfContent::LBR;

            }

            traceReader.advance ();

        }

    }

    return PerfContent::Unknown;

}

namespace {

    std::string parseFuncNameTillSlash (const std::string& str, std::size_t* i) {

        const std::size_t beg = *i;
              std::size_t end = beg;

        const std::size_t strSize = str.size ();
        while (str [end] != '/' && end < strSize)
            ++end;

        std::string res (str, beg, end - beg);
        *i = end + 1;

        return res;

    }

    class uint64_t_from_hex { // For use with boost::lexical_cast
        std::uint64_t value;
    public:
        operator std::uint64_t () const { return static_cast<std::uint64_t> (value); }

        friend std::istream& operator>> (std::istream& in, uint64_t_from_hex& outValue)
        {
            in >> std::hex >> outValue.value;
            return in;
        }
    };

}

struct LbrSample {

    std::uint64_t   callerOffset_;

    std::string     callerName_;
    std::string     calleeName_;

    LbrSample (const std::string& src, const std::string& dst)
    {
        std::vector<std::string> splitted;
        boost::split (splitted, dst, boost::is_any_of ("+"), boost::token_compress_on);
        calleeName_ = splitted [0];

        boost::split (splitted, src, boost::is_any_of ("+"), boost::token_compress_on);

        callerName_ = splitted [0];
        callerOffset_ = boost::lexical_cast<uint64_t_from_hex> (splitted [1]);
    }

};

void lbrSampleReParse (std::vector<LbrSample>& res,
                       const std::vector<std::pair<std::string, std::string>>& pre) {

    auto i = 0;
    auto preSize = pre.size ();
    while (i < preSize) {

        LbrSample newLbr (pre [i].first, pre [i].second);
        res.push_back (newLbr);
        ++i;

    }

}

void lbrPreParse (std::vector<std::pair<std::string, std::string>>& preRecord, const std::string& str) {
    
    std::size_t strSize = str.size ();
    std::size_t i = 0;
    while (i < strSize) {

        std::string caller = parseFuncNameTillSlash (str, &i);
        std::string callee = parseFuncNameTillSlash (str, &i);

        while (str [i] != ' ' && i < strSize)
            ++i;

        preRecord.push_back ({caller, callee});

        ++i;

    }

}

}

#endif
