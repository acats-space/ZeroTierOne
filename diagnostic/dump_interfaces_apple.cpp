#include "diagnostic/dump_interfaces.hpp"
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

void dumpInterfaces(std::stringstream& dump) {
    CFArrayRef interfaces = SCNetworkInterfaceCopyAll();
    CFIndex size = CFArrayGetCount(interfaces);
    for(CFIndex i = 0; i < size; ++i) {
        SCNetworkInterfaceRef iface = (SCNetworkInterfaceRef)CFArrayGetValueAtIndex(interfaces, i);
        dump << "Interface " << i << "\n-----------\n";
        CFStringRef tmp = SCNetworkInterfaceGetBSDName(iface);
        char stringBuffer[512] = {};
        CFStringGetCString(tmp,stringBuffer, sizeof(stringBuffer), kCFStringEncodingUTF8);
        dump << "Name: " << stringBuffer << "\n";
        std::string ifName(stringBuffer);
        int mtuCur, mtuMin, mtuMax;
        SCNetworkInterfaceCopyMTU(iface, &mtuCur, &mtuMin, &mtuMax);
        dump << "MTU: " << mtuCur << "\n";
        tmp = SCNetworkInterfaceGetHardwareAddressString(iface);
        CFStringGetCString(tmp, stringBuffer, sizeof(stringBuffer), kCFStringEncodingUTF8);
        dump << "MAC: " << stringBuffer << "\n";
        tmp = SCNetworkInterfaceGetInterfaceType(iface);
        CFStringGetCString(tmp, stringBuffer, sizeof(stringBuffer), kCFStringEncodingUTF8);
        dump << "Type: " << stringBuffer << "\n";
        dump << "Addresses:" << "\n";
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
                dump << stringBuffer << "\n";
            }
        }
        dump << "\n";
    }
} 