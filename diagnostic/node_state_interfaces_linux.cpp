#include "diagnostic/node_state_interfaces_linux.hpp"
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <vector>

void addNodeStateInterfacesJson(nlohmann::json& j) {
    try {
        std::vector<nlohmann::json> interfaces_json;
        struct ifreq ifr;
        struct ifconf ifc;
        char buf[1024];
        char stringBuffer[128];
        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        ifc.ifc_len = sizeof(buf);
        ifc.ifc_buf = buf;
        ioctl(sock, SIOCGIFCONF, &ifc);
        struct ifreq *it = ifc.ifc_req;
        const struct ifreq * const end = it + (ifc.ifc_len / sizeof(struct ifreq));
        for(; it != end; ++it) {
            strcpy(ifr.ifr_name, it->ifr_name);
            if(ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
                if (!(ifr.ifr_flags & IFF_LOOPBACK)) { // skip loopback
                    nlohmann::json iface_json;
                    iface_json["name"] = ifr.ifr_name;
                    if (ioctl(sock, SIOCGIFMTU, &ifr) == 0) {
                        iface_json["mtu"] = ifr.ifr_mtu;
                    }
                    if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
                        unsigned char mac_addr[6];
                        memcpy(mac_addr, ifr.ifr_hwaddr.sa_data, 6);
                        char macStr[18];
                        sprintf(macStr, "%02x:%02x:%02x:%02x:%02x:%02x",
                                mac_addr[0],
                                mac_addr[1],
                                mac_addr[2],
                                mac_addr[3],
                                mac_addr[4],
                                mac_addr[5]);
                        iface_json["mac"] = macStr;
                    }
                    std::vector<std::string> addresses;
                    struct ifaddrs *ifap, *ifa;
                    void *addr;
                    getifaddrs(&ifap);
                    for(ifa = ifap; ifa; ifa = ifa->ifa_next) {
                        if(strcmp(ifr.ifr_name, ifa->ifa_name) == 0 && ifa->ifa_addr != NULL) {
                            if(ifa->ifa_addr->sa_family == AF_INET) {
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
            }
        }
        close(sock);
        j["network_interfaces"] = interfaces_json;
    } catch (const std::exception& e) {
        j["network_interfaces"] = std::string("Exception: ") + e.what();
    } catch (...) {
        j["network_interfaces"] = "Unknown error retrieving interfaces";
    }
} 