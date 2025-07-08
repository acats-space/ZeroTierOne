#include "diagnostic/node_state_interfaces_apple.hpp"
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <vector>

void addNodeStateInterfacesJson(nlohmann::json& j) {
    try {
        std::vector<nlohmann::json> interfaces_json;
        CFArrayRef interfaces = SCNetworkInterfaceCopyAll();
        CFIndex size = CFArrayGetCount(interfaces);
        for(CFIndex i = 0; i < size; ++i) {
            SCNetworkInterfaceRef iface = (SCNetworkInterfaceRef)CFArrayGetValueAtIndex(interfaces, i);
            char stringBuffer[512] = {};
            CFStringRef tmp = SCNetworkInterfaceGetBSDName(iface);
            CFStringGetCString(tmp,stringBuffer, sizeof(stringBuffer), kCFStringEncodingUTF8);
            std::string ifName(stringBuffer);
            int mtuCur, mtuMin, mtuMax;
            SCNetworkInterfaceCopyMTU(iface, &mtuCur, &mtuMin, &mtuMax);
            nlohmann::json iface_json;
            iface_json["name"] = ifName;
            iface_json["mtu"] = mtuCur;
            tmp = SCNetworkInterfaceGetHardwareAddressString(iface);
            CFStringGetCString(tmp, stringBuffer, sizeof(stringBuffer), kCFStringEncodingUTF8);
            iface_json["mac"] = stringBuffer;
            tmp = SCNetworkInterfaceGetInterfaceType(iface);
            CFStringGetCString(tmp, stringBuffer, sizeof(stringBuffer), kCFStringEncodingUTF8);
            iface_json["type"] = stringBuffer;
            std::vector<std::string> addresses;
            struct ifaddrs *ifap, *ifa;
            void *addr;
            getifaddrs(&ifap);
            for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
                if (strcmp(ifName.c_str(), ifa->ifa_name) == 0) {
                    if (ifa->ifa_addr->sa_family == AF_INET) {
                        struct sockaddr_in *ipv4 = (struct sockaddr_in*)ifa->ifa_addr;
                        addr = &ipv4->sin_addr;
                    } else if (ifa->ifa_addr->sa_family == AF_INET6) {
                        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)ifa->ifa_addr;
                        addr = &ipv6->sin6_addr;
                    } else {
                        continue;
                    }
                    inet_ntop(ifa->ifa_addr->sa_family, addr, stringBuffer, sizeof(stringBuffer));
                    addresses.push_back(stringBuffer);
                }
            }
            iface_json["addresses"] = addresses;
            interfaces_json.push_back(iface_json);
        }
        j["network_interfaces"] = interfaces_json;
    } catch (const std::exception& e) {
        j["network_interfaces"] = std::string("Exception: ") + e.what();
    } catch (...) {
        j["network_interfaces"] = "Unknown error retrieving interfaces";
    }
} 