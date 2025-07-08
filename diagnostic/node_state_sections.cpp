#include "diagnostic/node_state_sections.hpp"
#include "osdep/Http.hpp"
#include "osdep/OSUtils.hpp"
#include <string>
#include <map>

void addNodeStateStatusJson(nlohmann::json& j, const ZeroTier::InetAddress& addr, std::map<std::string,std::string>& requestHeaders) {
    try {
        std::map<std::string, std::string> responseHeaders;
        std::string responseBody;
        unsigned int scode = ZeroTier::Http::GET(1024 * 1024 * 16,60000,(const struct sockaddr *)&addr,"/status",requestHeaders,responseHeaders,responseBody);
        if (scode == 200) {
            try {
                nlohmann::json status_json = ZeroTier::OSUtils::jsonParse(responseBody);
                j["status"] = status_json;
                if (status_json.contains("address")) {
                    j["nodeId"] = status_json["address"];
                } else {
                    j["nodeId"] = nullptr;
                }
            } catch (const std::exception& e) {
                j["status"] = { {"error", std::string("JSON parse error: ") + e.what()} };
                j["nodeId"] = nullptr;
            } catch (...) {
                j["status"] = { {"error", "Unknown JSON parse error"} };
                j["nodeId"] = nullptr;
            }
        } else {
            j["status"] = { {"error", std::string("HTTP error ") + std::to_string(scode) + ": " + responseBody} };
            j["nodeId"] = nullptr;
        }
    } catch (const std::exception& e) {
        j["status"] = { {"error", std::string("Exception: ") + e.what()} };
        j["nodeId"] = nullptr;
    } catch (...) {
        j["status"] = { {"error", "Unknown error retrieving /status"} };
        j["nodeId"] = nullptr;
    }
}

void addNodeStateNetworksJson(nlohmann::json& j, const ZeroTier::InetAddress& addr, std::map<std::string,std::string>& requestHeaders) {
    try {
        std::map<std::string, std::string> responseHeaders;
        std::string responseBody;
        unsigned int scode = ZeroTier::Http::GET(1024 * 1024 * 16,60000,(const struct sockaddr *)&addr,"/network",requestHeaders,responseHeaders,responseBody);
        if (scode == 200) {
            try {
                j["networks"] = ZeroTier::OSUtils::jsonParse(responseBody);
            } catch (...) {
                j["networks"] = responseBody;
            }
        } else {
            j["networks_error"] = responseBody;
        }
    } catch (const std::exception& e) {
        j["networks_error"] = std::string("Exception: ") + e.what();
    } catch (...) {
        j["networks_error"] = "Unknown error retrieving /network";
    }
}

void addNodeStatePeersJson(nlohmann::json& j, const ZeroTier::InetAddress& addr, std::map<std::string,std::string>& requestHeaders) {
    try {
        std::map<std::string, std::string> responseHeaders;
        std::string responseBody;
        unsigned int scode = ZeroTier::Http::GET(1024 * 1024 * 16,60000,(const struct sockaddr *)&addr,"/peer",requestHeaders,responseHeaders,responseBody);
        if (scode == 200) {
            try {
                j["peers"] = ZeroTier::OSUtils::jsonParse(responseBody);
            } catch (...) {
                j["peers"] = responseBody;
            }
        } else {
            j["peers_error"] = responseBody;
        }
    } catch (const std::exception& e) {
        j["peers_error"] = std::string("Exception: ") + e.what();
    } catch (...) {
        j["peers_error"] = "Unknown error retrieving /peer";
    }
}

void addNodeStateLocalConfJson(nlohmann::json& j, const std::string& homeDir) {
    try {
        std::string localConf;
        ZeroTier::OSUtils::readFile((homeDir + ZT_PATH_SEPARATOR_S + "local.conf").c_str(), localConf);
        if (localConf.empty()) {
            j["local_conf"] = nullptr;
        } else {
            j["local_conf"] = localConf;
        }
    } catch (const std::exception& e) {
        j["local_conf"] = std::string("Exception: ") + e.what();
    } catch (...) {
        j["local_conf"] = "Unknown error retrieving local.conf";
    }
} 