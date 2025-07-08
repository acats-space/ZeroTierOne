#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <map>
#include "node/InetAddress.hpp"

void addNodeStateStatusJson(nlohmann::json& j, const ZeroTier::InetAddress& addr, std::map<std::string,std::string>& requestHeaders);
void addNodeStateNetworksJson(nlohmann::json& j, const ZeroTier::InetAddress& addr, std::map<std::string,std::string>& requestHeaders);
void addNodeStatePeersJson(nlohmann::json& j, const ZeroTier::InetAddress& addr, std::map<std::string,std::string>& requestHeaders);
void addNodeStateLocalConfJson(nlohmann::json& j, const std::string& homeDir); 