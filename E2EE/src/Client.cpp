#include "Client.h"

Client::Client(const std::string& ip, int port)
	: m_ip(ip), m_port(port), m_serverSock(INVALID_SOCKET) {
	// Genera par de claves RSA al instanciar
	m_crypto.GenerateRSAKeys();
	// Genera la clave AES que se usará para cifrar mensajes
	m_crypto.GenerateAESKey();
}

Client::~Client() {
	if (m_serverSock != INVALID_SOCKET) {
		m_net.close(m_serverSock);
	}
}

bool
Client::Connect() {
	std::cout << "[Client] Conectando al servidor " << m_ip << ":" << m_port << "...\n";
	bool connected = m_net.ConnectToServer(m_ip, m_port);
	if (connected) {
		m_serverSock = m_net.m_serverSocket; // Guardar el socket una vez conectado
		std::cout << "[Client] Conexión establecida.\n";
	}
	else {
		std::cerr << "[Client] Error al conectar.\n";
	}
	return connected;
}

void
Client::ExchangeKeys() {
	// 1. Recibe la clave pública del servidor
	std::string serverPubKey = m_net.ReceiveData(m_serverSock);
	m_crypto.LoadPeerPublicKey(serverPubKey);
	std::cout << "[Client] Clave pública del servidor recibida.\n";

	// 2. Envía la clave pública del cliente
	std::string clientPubKey = m_crypto.GetPublicKeyString();
	m_net.SendData(m_serverSock, clientPubKey);
	std::cout << "[Client] Clave pública del cliente enviada.\n";
}

void
Client::SendAESKeyEncrypted() {
	std::vector<unsigned char> encryptedAES = m_crypto.EncryptAESKeyWithPeer();
	m_net.SendData(m_serverSock, encryptedAES);
	std::cout << "[Client] Clave AES cifrada y enviada al servidor.\n";
}

void
Client::SendEncryptedMessage(const std::string& message) {
	std::vector<unsigned char> iv;
	std::vector<unsigned char> encryptedMsg = m_crypto.AESEncrypt(message, iv);

	// Enviar IV y luego el mensaje cifrado
	m_net.SendData(m_serverSock, iv);
	m_net.SendData(m_serverSock, encryptedMsg);
	std::cout << "[Client] Mensaje cifrado enviado.\n";
}