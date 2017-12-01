#pragma once

// system headers
#include <algorithm>
#include <climits>
#include <cstring>
#include <grp.h>
#include <fstream>
#include <pwd.h>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
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
            return access(path.c_str(), F_OK) != -1;
        }

        static bool isFileOrDirectoryWritable(const std::string& path, bool& isWritable, bool forRoot = false) {
            struct stat st;
            if (stat(path.c_str(), &st) == -1) {
                auto error = errno;
                std::cerr << "Failed to call stat(): " << strerror(error);
                return false;
            }

            uid_t uid;

            if (forRoot) {
                uid = 0;
            } else {
                uid = getuid();
            }

            // first attempt: user owns file/directory and may write to it -> writable
            // (if user can write to file, root can write to it, too, by the way)
            if ((st.st_uid == uid || uid == 0) && st.st_mode & 0200) {
                isWritable = true;
                return true;
            }

            // second attempt: get user groups, check whether one of them has write access to file/directory -> writable
            std::vector<gid_t> gids;
            int ngroups = 0;

            // http://pubs.opengroup.org/onlinepubs/009695399/functions/getpwuid.html -> should reset errno
            errno = 0;
            struct passwd* pwd;
            if ((pwd = getpwuid(uid)) == nullptr) {
                auto error = errno;
                std::cerr << "Failed to call getpwuid(): " << strerror(error) << std::endl;
                return false;
            }

            // detect how many elements will be returned, and reserve that many elements in the vector
            getgrouplist(pwd->pw_name, pwd->pw_gid, gids.data(), &ngroups);

            if (ngroups <= 0) {
                std::cerr << "Failed to get number of gids for user " << uid << std::endl;
                return false;
            }

            gids.resize(static_cast<unsigned long>(ngroups));

            // now, really get group IDs
            if (getgrouplist(pwd->pw_name, pwd->pw_gid, gids.data(), &ngroups) == -1) {
                auto error = errno;
                std::cerr << "Failed to call getgrouplist(): " << strerror(error) << std::endl;
                return false;
            }

            // check whether one of those groups has write access
            for (const auto gid : gids) {
                if (st.st_gid == gid && st.st_mode & 0020) {
                    isWritable = true;
                    return true;
                }
            }

            // thid attempt: everyone may write to the file/directory (unlike, but you never know) -> writable
            if (st.st_mode & 0002) {
                isWritable = true;
                return true;
            }

            // well, file/directory is just not writable
            isWritable = false;
            return true;
        }

        static long long gidForUid(uid_t uid) {
            // http://pubs.opengroup.org/onlinepubs/009695399/functions/getpwuid.html -> should reset errno
            errno = 0;

            struct passwd* pwd;

            if ((pwd = getpwuid(uid)) == nullptr) {
                auto error = errno;
                std::cerr << "Failed to call getpwuid(): " << strerror(error) << std::endl;
                return -1;
            }

            return pwd->pw_gid;
        }
    }
}
