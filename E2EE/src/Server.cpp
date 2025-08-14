#include "Server.h"

Server::Server(int port) : m_port(port), m_clientSock(-1) {
	// Generar claves RSA al construir
	m_crypto.GenerateRSAKeys();
}

Server::~Server() {
	// Cerrar conexión con el cliente si aún está activa
	if (m_clientSock != -1) {
		m_net.close(m_clientSock);
	}
}


bool Server::Start() {
	std::cout << "[Server] Iniciando servidor en el puerto " << m_port << "...\n";
	return m_net.StartServer(m_port);
}


void Server::WaitForClient() {
	std::cout << "[Server] Esperando conexión de un cliente...\n";

	// Aceptar conexión entrante
	m_clientSock = m_net.AcceptClient();
	if (m_clientSock == INVALID_SOCKET) {
		std::cerr << "[Server] No se pudo aceptar cliente.\n";
		return;
	}
	std::cout << "[Server] Cliente conectado.\n";

	// 1. Enviar clave pública del servidor al cliente
	std::string serverPubKey = m_crypto.GetPublicKeyString();
	m_net.SendData(m_clientSock, serverPubKey);

	// 2. Recibir clave pública del cliente
	std::string clientPubKey = m_net.ReceiveData(m_clientSock);
	m_crypto.LoadPeerPublicKey(clientPubKey);

	// 3. Recibir clave AES cifrada con la pública del servidor
	std::vector<unsigned char> encryptedAESKey = m_net.ReceiveDataBinary(m_clientSock, 256);
	m_crypto.DecryptAESKey(encryptedAESKey);

	std::cout << "[Server] Clave AES intercambiada exitosamente.\n";
}


void Server::ReceiveEncryptedMessage() {
	// 1. Recibir el IV (vector de inicialización)
	std::vector<unsigned char> iv = m_net.ReceiveDataBinary(m_clientSock, 16);

	// 2. Recibir el mensaje cifrado
	std::vector<unsigned char> encryptedMsg = m_net.ReceiveDataBinary(m_clientSock, 128);

	// 3. Descifrar el mensaje
	std::string msg = m_crypto.AESDecrypt(encryptedMsg, iv);

	// 4. Mostrar mensaje
	std::cout << "[Server] Mensaje recibido: " << msg << "\n";
}