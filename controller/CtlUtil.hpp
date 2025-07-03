#ifndef ZT_CTLUTIL_HPP
#define ZT_CTLUTIL_HPP

#include <vector>
#include <string>

namespace ZeroTier {

    const char *_timestr();

    std::vector<std::string> split(std::string str, char delim);

    std::string url_encode(const std::string &value);
}

#endif // namespace ZeroTier