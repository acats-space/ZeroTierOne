#include "diagnostic/dump_interfaces.hpp"
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

void dumpInterfaces(std::stringstream& dump) {
    struct ifaddrs *ifap, *ifa;
    if (getifaddrs(&ifap) != 0) {
        dump << "ERROR: getifaddrs failed\n";
        return;
    }
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        dump << "Interface: " << ifa->ifa_name << "\n";
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock >= 0) {
            struct ifreq ifr;
            strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ);
            if (ioctl(sock, SIOCGIFMTU, &ifr) == 0) {
                dump << "MTU: " << ifr.ifr_mtu << "\n";
            }
            if (ifa->ifa_addr->sa_family == AF_LINK) {
                struct sockaddr_dl* sdl = (struct sockaddr_dl*)ifa->ifa_addr;
                unsigned char* mac = (unsigned char*)LLADDR(sdl);
                char macStr[32];
                snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                dump << "MAC: " << macStr << "\n";
            }
            close(sock);
        }
        dump << "Addresses:\n";
        if (ifa->ifa_addr->sa_family == AF_INET) {
            char addr[INET_ADDRSTRLEN];
            struct sockaddr_in* sa = (struct sockaddr_in*)ifa->ifa_addr;
            inet_ntop(AF_INET, &(sa->sin_addr), addr, INET_ADDRSTRLEN);
            dump << addr << "\n";
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            char addr[INET6_ADDRSTRLEN];
            struct sockaddr_in6* sa6 = (struct sockaddr_in6*)ifa->ifa_addr;
            inet_ntop(AF_INET6, &(sa6->sin6_addr), addr, INET6_ADDRSTRLEN);
            dump << addr << "\n";
        }
        dump << "\n";
    }
    freeifaddrs(ifap);
} 