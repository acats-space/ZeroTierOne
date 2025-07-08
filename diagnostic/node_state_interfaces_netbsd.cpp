#include "diagnostic/node_state_interfaces_netbsd.hpp"
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <vector>

void addNodeStateInterfacesJson(nlohmann::json& j) {
    try {
        std::vector<nlohmann::json> interfaces_json;
        struct ifaddrs *ifap, *ifa;
        if (getifaddrs(&ifap) != 0) {
            j["network_interfaces"] = "ERROR: getifaddrs failed";
            return;
        }
        for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) continue;
            nlohmann::json iface_json;
            iface_json["name"] = ifa->ifa_name;
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock >= 0) {
                struct ifreq ifr;
                strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ);
                if (ioctl(sock, SIOCGIFMTU, &ifr) == 0) {
                    iface_json["mtu"] = ifr.ifr_mtu;
                }
                if (ifa->ifa_addr->sa_family == AF_LINK) {
                    struct sockaddr_dl* sdl = (struct sockaddr_dl*)ifa->ifa_addr;
                    unsigned char* mac = (unsigned char*)LLADDR(sdl);
                    char macStr[32];
                    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
                        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                    iface_json["mac"] = macStr;
                }
                close(sock);
            }
            std::vector<std::string> addresses;
            if (ifa->ifa_addr->sa_family == AF_INET) {
                char addr[INET_ADDRSTRLEN];
                struct sockaddr_in* sa = (struct sockaddr_in*)ifa->ifa_addr;
                inet_ntop(AF_INET, &(sa->sin_addr), addr, INET_ADDRSTRLEN);
                addresses.push_back(addr);
            } else if (ifa->ifa_addr->sa_family == AF_INET6) {
                char addr[INET6_ADDRSTRLEN];
                struct sockaddr_in6* sa6 = (struct sockaddr_in6*)ifa->ifa_addr;
                inet_ntop(AF_INET6, &(sa6->sin6_addr), addr, INET6_ADDRSTRLEN);
                addresses.push_back(addr);
            }
            iface_json["addresses"] = addresses;
            interfaces_json.push_back(iface_json);
        }
        freeifaddrs(ifap);
        j["network_interfaces"] = interfaces_json;
    } catch (const std::exception& e) {
        j["network_interfaces"] = std::string("Exception: ") + e.what();
    } catch (...) {
        j["network_interfaces"] = "Unknown error retrieving interfaces";
    }
} 