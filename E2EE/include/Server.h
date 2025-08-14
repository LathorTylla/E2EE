#pragma once
#include "NetworkHelper.h"
#include "CryptoHelper.h"
#include "Prerequisites.h"

class
Server {
public:
	Server() = default;
	Server(int port);

	~Server();

	bool 
	Start();

	void
  WaitForClient();

	void
	ReceiveEncryptedMessage();

private:

	int m_port;
	SOCKET m_clientSock;
	NetworkHelper m_net;
	CryptoHelper m_crypto;

};