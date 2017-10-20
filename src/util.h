#pragma once

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace appimage {
    namespace update {

        static void stripNewlineCharacters(std::string &str) {
            str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
        }

        static bool callProgramAndGrepForLine(const std::string command, const std::string pattern,
                                              std::string &output) {
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
                    stripNewlineCharacters(output);
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
    }
}