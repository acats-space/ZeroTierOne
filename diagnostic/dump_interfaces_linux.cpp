#include "diagnostic/dump_interfaces.hpp"
#include <sys/types.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sstream>

void dumpInterfaces(std::stringstream& dump) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    struct ifconf ifc;
    char buf[1024];
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    ioctl(sock, SIOCGIFCONF, &ifc);
    struct ifreq *it = ifc.ifc_req;
    const struct ifreq * const end = it + (ifc.ifc_len / sizeof(struct ifreq));
    for(; it != end; ++it) {
        struct ifreq ifr;
        strcpy(ifr.ifr_name, it->ifr_name);
        if(ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
            if (!(ifr.ifr_flags & IFF_LOOPBACK)) { // skip loopback
                dump << "Interface: " << ifr.ifr_name << "\n";
                if (ioctl(sock, SIOCGIFMTU, &ifr) == 0) {
                    dump << "MTU: " << ifr.ifr_mtu << "\n";
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
                    dump << "MAC: " << macStr << "\n";
                }
                dump << "Addresses:" << "\n";
                struct ifaddrs *ifap, *ifa;
                void *addr;
                getifaddrs(&ifap);
                for(ifa = ifap; ifa; ifa = ifa->ifa_next) {
                    if(strcmp(ifr.ifr_name, ifa->ifa_name) == 0 && ifa->ifa_addr != NULL) {
                        char stringBuffer[128];
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
                        dump << stringBuffer << "\n";
                    }
                }
                dump << "\n";
            }
        }
    }
    close(sock);
} 