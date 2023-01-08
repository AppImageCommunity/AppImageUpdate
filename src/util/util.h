#pragma once

// system headers
#include <string>
#include <vector>

namespace appimage::update::util {
    void removeNewlineCharacters(std::string& str);

    bool ltrim(std::string& s, char to_trim = ' ');

    bool rtrim(std::string& s, char to_trim = ' ');

    bool trim(std::string& s, char to_trim = ' ');

    std::vector<std::string> split(const std::string& s, char delim = ' ');

    std::string join(const std::vector<std::string>& strings, const std::string& delim = " ");

    std::string toLower(std::string s);

    bool toLong(const std::string& str, long& retval, int base = 10);

    bool isFile(const std::string& path);

    void copyPermissions(const std::string& oldPath, const std::string& newPath);

    void runApp(const std::string& path);

    // Reads an ELF file section and returns its contents.
    std::string readElfSection(const std::string& filePath, const std::string& sectionName);

    std::string findInPATH(const std::string& name);

    bool stringStartsWith(const std::string& string, const std::string& prefix);

    std::string abspath(const std::string& path);

    std::string pathToOldAppImage(const std::string& oldPath, const std::string& newPath);;

    // workaround for AppImageLauncher limitation, see https://github.com/AppImage/AppImageUpdate/issues/131
    std::string ailfsRealpath(const std::string& path);

    std::vector<char> makeBuffer(const std::string& str);
};
