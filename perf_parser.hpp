#ifndef PERF_PARSER_HPP__
#define PERF_PARSER_HPP__

#include <iostream>
#include <fstream>
#include "myString.hpp"

namespace perfParser {

class TraceStream final {
    ISP::StringWrapper currentLine_;
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
    ISP::StringWrapper getCurrentLine () const {
        
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
// leading "  0x" and the '/' exist.
bool isLBRSample (const ISP::StringWrapper& str) {

    std::vector<ISP::StringWrapper> record;
    str.trim_copy ().split (record, " ");

    if (record.size () != 2)
        return false;
    if (record [1].startsWith ("0x") && record [1].contains ('/'))
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
        int res = 0;
        while (!traceReader.isAtEOF () && traceReader.getCurrentLine ().ltrim_copy ().getAsNumber (16, res)) {
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

}

#endif
