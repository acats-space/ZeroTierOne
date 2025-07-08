#pragma once
#include <string>
#include <map>
#include <nlohmann/json.hpp>
#include "node/InetAddress.hpp"

void write_node_state_json(const ZeroTier::InetAddress &addr, const std::string &homeDir, std::map<std::string, std::string> &requestHeaders, std::map<std::string, std::string> &responseHeaders, std::string &responseBody); 