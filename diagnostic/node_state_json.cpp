#include "version.h"
#include "diagnostic/node_state_json.hpp"
#include "diagnostic/node_state_sections.hpp"
#include "diagnostic/node_state_interfaces_linux.hpp" // platform-specific, add others as needed
#include <nlohmann/json.hpp>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>

namespace {
std::string make_timestamp() {
    auto t = std::time(nullptr);
    std::tm tm_utc = *std::gmtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%dT%H%M%SZ", &tm_utc);
    return std::string(buf);
}
}

void write_node_state_json(const ZeroTier::InetAddress &addr, const std::string &homeDir, std::map<std::string, std::string> &requestHeaders, std::map<std::string, std::string> &responseHeaders, std::string &responseBody) {
    nlohmann::json j;
    // Schema version for MCP/diagnostic output
    j["schema_version"] = "1.0"; // Update this if the schema changes
    std::vector<std::string> errors;

    // Timestamps
    auto t = std::time(nullptr);
    auto tm_utc = *std::gmtime(&t);
    auto tm_local = *std::localtime(&t);
    std::stringstream utc_ts, local_ts;
    utc_ts << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
    local_ts << std::put_time(&tm_local, "%Y-%m-%dT%H:%M:%S%z");
    j["utc_timestamp"] = utc_ts.str();
    j["local_timestamp"] = local_ts.str();

#ifdef __APPLE__
    j["platform"] = "macOS";
#elif defined(_WIN32)
    j["platform"] = "Windows";
#elif defined(__linux__)
    j["platform"] = "Linux";
#else
    j["platform"] = "other unix based OS";
#endif
    j["zerotier_version"] = std::to_string(ZEROTIER_ONE_VERSION_MAJOR) + "." + std::to_string(ZEROTIER_ONE_VERSION_MINOR) + "." + std::to_string(ZEROTIER_ONE_VERSION_REVISION);

    // Extensibility/context fields
    // node_role: placeholder (could be "controller", "member", etc.)
    j["node_role"] = nullptr; // Set to actual role if available
    // uptime: seconds since boot (best effort)
    long uptime = -1;
    #ifdef __linux__
    FILE* f = fopen("/proc/uptime", "r");
    if (f) {
        if (fscanf(f, "%ld", &uptime) != 1) uptime = -1;
        fclose(f);
    }
    #endif
    if (uptime >= 0)
        j["uptime"] = uptime;
    else
        j["uptime"] = nullptr;
    // hostname
    char hostname[256] = {};
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        j["hostname"] = hostname;
    } else {
        j["hostname"] = nullptr;
    }
    // tags: extensibility array for future use (e.g., MCP tags, custom info)
    j["tags"] = nlohmann::json::array();
    // mcp_context: extensibility object for MCP or plugin context
    j["mcp_context"] = nlohmann::json::object();

    // Add each section
    try {
        addNodeStateStatusJson(j, addr, requestHeaders);
    } catch (const std::exception& e) {
        errors.push_back(std::string("status section: ") + e.what());
    } catch (...) {
        errors.push_back("status section: unknown error");
    }
    try {
        addNodeStateNetworksJson(j, addr, requestHeaders);
    } catch (const std::exception& e) {
        errors.push_back(std::string("networks section: ") + e.what());
    } catch (...) {
        errors.push_back("networks section: unknown error");
    }
    try {
        addNodeStatePeersJson(j, addr, requestHeaders);
    } catch (const std::exception& e) {
        errors.push_back(std::string("peers section: ") + e.what());
    } catch (...) {
        errors.push_back("peers section: unknown error");
    }
    try {
        addNodeStateLocalConfJson(j, homeDir);
    } catch (const std::exception& e) {
        errors.push_back(std::string("local_conf section: ") + e.what());
    } catch (...) {
        errors.push_back("local_conf section: unknown error");
    }
    try {
        addNodeStateInterfacesJson(j); // platform-specific
    } catch (const std::exception& e) {
        errors.push_back(std::string("interfaces section: ") + e.what());
    } catch (...) {
        errors.push_back("interfaces section: unknown error");
    }
    j["errors"] = errors;

    // Filename: nodeId and timestamp
    std::string nodeId = (j.contains("nodeId") && j["nodeId"].is_string()) ? j["nodeId"].get<std::string>() : "unknown";
    std::string timestamp = make_timestamp();
    std::string filename = "zerotier_node_state_" + nodeId + "_" + timestamp + ".json";
    std::string tmp_path = "/tmp/" + filename;
    std::string cwd_path = filename;
    std::string json_str = j.dump(2);

    // Try /tmp, then cwd, then stdout
    bool written = false;
    {
        std::ofstream ofs(tmp_path);
        if (ofs) {
            ofs << json_str;
            ofs.close();
            std::cout << "Wrote node state to: " << tmp_path << std::endl;
            written = true;
        }
    }
    if (!written) {
        std::ofstream ofs(cwd_path);
        if (ofs) {
            ofs << json_str;
            ofs.close();
            std::cout << "Wrote node state to: " << cwd_path << std::endl;
            written = true;
        }
    }
    if (!written) {
        std::cout << json_str << std::endl;
        std::cerr << "Could not write node state to file, output to stdout instead." << std::endl;
    }
} 