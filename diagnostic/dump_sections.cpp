#include "diagnostic/dump_sections.hpp"
#include "node/InetAddress.hpp"
#include "osdep/OSUtils.hpp"
#include "osdep/Http.hpp"
#include <sstream>
#include <string>
#include <map>

void dumpStatus(std::stringstream& dump, const ZeroTier::InetAddress& addr, std::map<std::string,std::string>& requestHeaders) {
    dump << "status" << ZT_EOL_S << "------" << ZT_EOL_S;
    std::map<std::string, std::string> responseHeaders;
    std::string responseBody;
    unsigned int scode = ZeroTier::Http::GET(1024 * 1024 * 16,60000,(const struct sockaddr *)&addr,"/status",requestHeaders,responseHeaders,responseBody);
    if (scode != 200) {
        dump << responseBody << ZT_EOL_S;
        return;
    }
    dump << responseBody << ZT_EOL_S;
}

void dumpNetworks(std::stringstream& dump, const ZeroTier::InetAddress& addr, std::map<std::string,std::string>& requestHeaders) {
    dump << ZT_EOL_S << "networks" << ZT_EOL_S << "--------" << ZT_EOL_S;
    std::map<std::string, std::string> responseHeaders;
    std::string responseBody;
    unsigned int scode = ZeroTier::Http::GET(1024 * 1024 * 16,60000,(const struct sockaddr *)&addr,"/network",requestHeaders,responseHeaders,responseBody);
    if (scode != 200) {
        dump << responseBody << ZT_EOL_S;
        return;
    }
    dump << responseBody << ZT_EOL_S;
}

void dumpPeers(std::stringstream& dump, const ZeroTier::InetAddress& addr, std::map<std::string,std::string>& requestHeaders) {
    dump << ZT_EOL_S << "peers" << ZT_EOL_S << "-----" << ZT_EOL_S;
    std::map<std::string, std::string> responseHeaders;
    std::string responseBody;
    unsigned int scode = ZeroTier::Http::GET(1024 * 1024 * 16,60000,(const struct sockaddr *)&addr,"/peer",requestHeaders,responseHeaders,responseBody);
    if (scode != 200) {
        dump << responseBody << ZT_EOL_S;
        return;
    }
    dump << responseBody << ZT_EOL_S;
}

void dumpLocalConf(std::stringstream& dump, const std::string& homeDir) {
    dump << ZT_EOL_S << "local.conf" << ZT_EOL_S << "----------" << ZT_EOL_S;
    std::string localConf;
    ZeroTier::OSUtils::readFile((homeDir + ZT_PATH_SEPARATOR_S + "local.conf").c_str(), localConf);
    if (localConf.empty()) {
        dump << "None Present" << ZT_EOL_S;
    }
    else {
        dump << localConf << ZT_EOL_S;
    }
} 