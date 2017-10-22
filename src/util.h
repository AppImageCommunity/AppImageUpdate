#pragma once

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace appimage {
    namespace update {
        static void removeNewlineCharacters(std::string& str) {
            str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
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

        static bool callProgramAndGrepForLine(const std::string& command, const std::string& pattern,
                                              std::string& output) {
            FILE *stream = popen(command.c_str(), "r");

            if (stream == nullptr)
                return false;

            char *line;
            size_t lineSize = 0;
            while(getline(&line, &lineSize, stream)) {
                // check whether line matches pattern
                std::string lineString = line;
                if (lineString.find(pattern) != std::string::npos) {
                    if (pclose(stream) != 0) {
                        free(line);
                        return false;
                    }
                    output = line;
                    removeNewlineCharacters(output);
                    return true;
                }
            }

            if (pclose(stream) != 0) {
                free(line);
                return false;
            }

            return false;
        }

        static std::vector<std::string> split(const std::string& s, char delim = ' ') {
            std::vector<std::string> result;

            std::stringstream ss(s);
            std::string item;

            while (std::getline(ss, item, delim)) {
                result.push_back(item);
            }

            return result;
        }

        static std::string toLower(std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
            return s;
        }
    }
}