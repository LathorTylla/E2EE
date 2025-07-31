#pragma once
#include "Prerequisites"
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

class NetworkHelper
{
public:
	NetworkHelper();
	~NetworkHelper();

private:

};

bool StartServer(int port)

SOCKET AcceptClient();

bool ConnectToServer(const std::string&ip, int port);