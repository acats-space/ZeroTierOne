#include "diagnostic/node_state_interfaces_win32.hpp"
#include <windows.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <vector>

void addNodeStateInterfacesJson(nlohmann::json& j) {
    try {
        std::vector<nlohmann::json> interfaces_json;
        ULONG buffLen = 16384;
        PIP_ADAPTER_ADDRESSES addresses;
        ULONG ret = 0;
        do {
            addresses = (PIP_ADAPTER_ADDRESSES)malloc(buffLen);
            ret = GetAdaptersAddresses(AF_UNSPEC, 0, NULL, addresses, &buffLen);
            if (ret == ERROR_BUFFER_OVERFLOW) {
                free(addresses);
                addresses = NULL;
            } else {
                break;
            }
        } while (ret == ERROR_BUFFER_OVERFLOW);
        if (ret == NO_ERROR) {
            PIP_ADAPTER_ADDRESSES curAddr = addresses;
            while (curAddr) {
                nlohmann::json iface_json;
                iface_json["name"] = curAddr->AdapterName;
                iface_json["mtu"] = curAddr->Mtu;
                char macBuffer[64] = {};
                sprintf(macBuffer, "%02x:%02x:%02x:%02x:%02x:%02x",
                    curAddr->PhysicalAddress[0],
                    curAddr->PhysicalAddress[1],
                    curAddr->PhysicalAddress[2],
                    curAddr->PhysicalAddress[3],
                    curAddr->PhysicalAddress[4],
                    curAddr->PhysicalAddress[5]);
                iface_json["mac"] = macBuffer;
                iface_json["type"] = curAddr->IfType;
                std::vector<std::string> addresses;
                PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;
                pUnicast = curAddr->FirstUnicastAddress;
                if (pUnicast) {
                    for (int j = 0; pUnicast != NULL; ++j) {
                        char buf[128] = {};
                        DWORD bufLen = 128;
                        LPSOCKADDR a = pUnicast->Address.lpSockaddr;
                        WSAAddressToStringA(
                            pUnicast->Address.lpSockaddr,
                            pUnicast->Address.iSockaddrLength,
                            NULL,
                            buf,
                            &bufLen
                        );
                        addresses.push_back(buf);
                        pUnicast = pUnicast->Next;
                    }
                }
                iface_json["addresses"] = addresses;
                interfaces_json.push_back(iface_json);
                curAddr = curAddr->Next;
            }
        }
        if (addresses) {
            free(addresses);
            addresses = NULL;
        }
        j["network_interfaces"] = interfaces_json;
    } catch (const std::exception& e) {
        j["network_interfaces"] = std::string("Exception: ") + e.what();
    } catch (...) {
        j["network_interfaces"] = "Unknown error retrieving interfaces";
    }
} 