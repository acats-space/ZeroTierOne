#include "diagnostic/dump_interfaces.hpp"
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <string>

void dumpInterfaces(std::stringstream& dump) {
    ULONG buffLen = 16384;
    PIP_ADAPTER_ADDRESSES addresses;
    ULONG ret = 0;
    do {
        addresses = (PIP_ADAPTER_ADDRESSES)malloc(buffLen);
        ret = GetAdaptersAddresses(AF_UNSPEC, 0, NULL, addresses, &buffLen);
        if (ret == ERROR_BUFFER_OVERFLOW) {
            free(addresses);
            addresses = NULL;
        }
        else {
            break;
        }
    } while (ret == ERROR_BUFFER_OVERFLOW);
    int i = 0;
    if (ret == NO_ERROR) {
        PIP_ADAPTER_ADDRESSES curAddr = addresses;
        while (curAddr) {
            dump << "Interface " << i << "\n-----------\n";
            dump << "Name: " << curAddr->AdapterName << "\n";
            dump << "MTU: " << curAddr->Mtu << "\n";
            char macBuffer[64] = {};
            sprintf(macBuffer, "%02x:%02x:%02x:%02x:%02x:%02x",
                curAddr->PhysicalAddress[0],
                curAddr->PhysicalAddress[1],
                curAddr->PhysicalAddress[2],
                curAddr->PhysicalAddress[3],
                curAddr->PhysicalAddress[4],
                curAddr->PhysicalAddress[5]);
            dump << "MAC: " << macBuffer << "\n";
            dump << "Type: " << curAddr->IfType << "\n";
            dump << "Addresses:" << "\n";
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
                    dump << buf << "\n";
                    pUnicast = pUnicast->Next;
                }
            }
            dump << "\n";
            curAddr = curAddr->Next;
        }
    }
    if (addresses) {
        free(addresses);
        addresses = NULL;
    }
} 