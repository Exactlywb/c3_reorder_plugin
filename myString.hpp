#ifndef MY_STRING_HPP__
#define MY_STRING_HPP__

#include <iostream>
#include <vector>
#include <cctype>
#include <algorithm>
#include <locale>
#include <cstring>

namespace ISP {

class StringWrapper final {

    std::string str_;

public:
    StringWrapper (const char* str):
        str_ (str) {}

    StringWrapper (std::string str):
        str_ (str) {}

    StringWrapper () = default;
    
    std::string getStr () const { return str_; }
    std::size_t size () const { return str_.size (); }

    inline StringWrapper ltrim_copy (const char* t = " \t\n\r\f\v") const
    {
        std::string res = str_;
        res.erase (0, res.find_first_not_of(t));
        return res;
    }

    inline StringWrapper rtrim_copy (const char* t = " \t\n\r\f\v") const
    {
        std::string res = str_;
        res.erase (res.find_last_not_of(t) + 1);
        return res;
    }

    inline StringWrapper trim_copy (const char* t = " \t\n\r\f\v") const
    {
        return ltrim_copy ().rtrim_copy ();
    }

    inline bool getAsNumber (const int base, int& res) const {

        if (base <= 1 || base > 36)
            throw std::runtime_error ("Bad base value");

        int resCalc = 0;

        std::size_t strSize = str_.size ();
        for (std::size_t i = 0; i < strSize; ++i) {

            if (str_ [i] >= '0' && str_ [i] <= '9')
                resCalc = (resCalc * base) + (str_ [i] - '0');
            else if (str_ [i] >= 'a' && str_ [i] <= 'z')
                resCalc = (resCalc * base) + (str_ [i] - 'a');
            else if (str_ [i] >= 'A' && str_ [i] <= 'Z')
                resCalc = (resCalc * base) + (str_ [i] - 'A');
            else
                return false;

        }
        
        res = resCalc;
        return true;
        
    }

    inline bool contains (const char symb) const {

        std::string::size_type foundSymbNum = str_.find (symb);
        if (foundSymbNum != std::string::npos)
            return true;

        return false;

    }

    inline bool startsWith (const std::string& subStr) const {

        return std::strncmp (subStr.c_str (), str_.c_str (), subStr.length ());

    }

    inline void split (std::vector<StringWrapper>& record, const char* symbs) const {

        std::string::size_type foundSymbNum = str_.find_first_of (symbs);
        if (foundSymbNum != std::string::npos) {

            record.push_back (std::string (str_, 0, foundSymbNum));
            record.push_back (std::string (str_, foundSymbNum, str_.length ()));

        }

    }

};

}

#endif
