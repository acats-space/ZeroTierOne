#pragma once
#include <sstream>
#include <string>
#include <map>
#include "node/InetAddress.hpp"

void dumpStatus(std::stringstream& dump, const ZeroTier::InetAddress& addr, std::map<std::string,std::string>& requestHeaders);
void dumpNetworks(std::stringstream& dump, const ZeroTier::InetAddress& addr, std::map<std::string,std::string>& requestHeaders);
void dumpPeers(std::stringstream& dump, const ZeroTier::InetAddress& addr, std::map<std::string,std::string>& requestHeaders);
void dumpLocalConf(std::stringstream& dump, const std::string& homeDir); 