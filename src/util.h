#pragma once

// system headers
#include <algorithm>
#include <climits>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "light_elf.h"

namespace appimage {
    namespace update {
        static std::vector<std::string> split(const std::string& s, char delim = ' ') {
            std::vector<std::string> result;

            std::stringstream ss(s);
            std::string item;

            while (std::getline(ss, item, delim)) {
                result.push_back(item);
            }

            return result;
        }
        static inline bool ltrim(std::string& s, char to_trim = ' ') {
            // TODO: find more efficient way to check whether elements have been removed
            size_t initialLength = s.length();
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [to_trim](int ch) {
                return ch != to_trim;
            }));
            return s.length() < initialLength;
        }
        
        static inline bool rtrim(std::string& s, char to_trim = ' ') {
            // TODO: find more efficient way to check whether elements have been removed
            auto initialLength = s.length();
            s.erase(std::find_if(s.rbegin(), s.rend(), [to_trim](int ch) {
                return ch != to_trim;
            }).base(), s.end());
            return s.length() < initialLength;
        }

        static inline bool trim(std::string& s, char to_trim = ' ') {
            // returns true if either modifies s
            auto ltrim_result = ltrim(s, to_trim);
            return rtrim(s, to_trim) && ltrim_result;
        }
        
        static inline std::string toLower(std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
            return s;
        }

        static inline bool toLong(const std::string& str, long& retval, int base = 10) {
            char* end = nullptr;
            const auto* cstr = str.c_str();

            auto rv = std::strtol(cstr, &end, base);
            if (errno == ERANGE || cstr == end || retval > LONG_MAX || retval < LONG_MIN)
                return false;

            retval = rv;
            return true;
        }

        static inline bool isFile(const std::string& path) {
            std::ifstream ifs(path);
            return (bool) ifs && ifs.good();
        }
    }
}
