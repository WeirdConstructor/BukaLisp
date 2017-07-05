#pragma once

inline std::string str_replace(std::string input, const std::string& search,
                          const std::string& replace) {
    size_t pos = 0;
    while((pos = input.find(search, pos))
          != std::string::npos)
    {
         input.replace(pos, search.length(), replace);
         pos += replace.length();
    }
    return input;
}
