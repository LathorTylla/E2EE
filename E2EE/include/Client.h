#pragma once
#include "NetworkHelper.h"
#include "CryptoHelper.h"
#include "Prerequisites.h"

class 
Client {
public:
	Client() = default;
	Client(const std::string& ip, int port);
	~Client();

	bool
	Connect();


	void
	ExchangeKeys();


	void
	SendAESKeyEncrypted();


	void
	SendEncryptedMessage(const std::string& message);

	void 
	SendEncryptedMessageLoop();

private:
	std::string m_ip;
	int m_port;
	SOCKET m_serverSock;
	NetworkHelper m_net;
	CryptoHelper m_crypto;
};