#include "Server.h"

// Constructor initializes the server with a port and generates RSA keys
Server::Server(int port) : m_port(port), m_clientSock(-1) {
	// Generate RSA keys on construction
	m_crypto.GenerateRSAKeys();
}

// Destructor cleans up client socket connection if active
Server::~Server() {
	// Close client connection if still active
	if (m_clientSock != -1) {
		m_net.close(m_clientSock);
	}
}

// Starts the server listening on the specified port
bool 
Server::Start() {
	std::cout << "[Server] Iniciando servidor en el puerto " << m_port << "...\n";
	return m_net.StartServer(m_port);
}

// Waits for a client connection and performs the secure key exchange
void 
Server::WaitForClient() {
	std::cout << "[Server] Esperando conexión de un cliente...\n";

	// Accept incoming connection
	m_clientSock = m_net.AcceptClient();
	if (m_clientSock == INVALID_SOCKET) {
		std::cerr << "[Server] No se pudo aceptar cliente.\n";
		return;
	}
	std::cout << "[Server] Cliente conectado.\n";

	// Key exchange protocol:
	// 1. Send server's public key to client
	std::string serverPubKey = m_crypto.GetPublicKeyString();
	m_net.SendData(m_clientSock, serverPubKey);

	// 2. Receive client's public key
	std::string clientPubKey = m_net.ReceiveData(m_clientSock);
	m_crypto.LoadPeerPublicKey(clientPubKey);

	// 3. Receive AES key encrypted with server's public key
	std::vector<unsigned char> encryptedAESKey = m_net.ReceiveDataBinary(m_clientSock, 256);
	m_crypto.DecryptAESKey(encryptedAESKey);

	std::cout << "[Server] Clave AES intercambiada exitosamente.\n";
}

// Receives and decrypts a single message from the client
void 
Server::ReceiveEncryptedMessage() {
	// 1. Receive initialization vector (IV)
	std::vector<unsigned char> iv = m_net.ReceiveDataBinary(m_clientSock, 16);

	// 2. Receive encrypted message
	std::vector<unsigned char> encryptedMsg = m_net.ReceiveDataBinary(m_clientSock, 128);

	// 3. Decrypt the message
	std::string msg = m_crypto.AESDecrypt(encryptedMsg, iv);

	// 4. Display message
	std::cout << "[Server] Mensaje recibido: " << msg << "\n";
}

// Continuously receives encrypted messages from the client
void
Server::StartReceiveLoop() {
	while (true) {
		// 1) Receive IV (16 bytes)
		auto iv = m_net.ReceiveDataBinary(m_clientSock, 16);
		if (iv.empty()) {
			std::cout << "\n[Server] Conexión cerrada por el cliente.\n";
			break;
		}

		// 2) Receive message length (4 bytes, network/big-endian)
		auto len4 = m_net.ReceiveDataBinary(m_clientSock, 4);
		if (len4.size() != 4) {
			std::cout << "[Server] Error al recibir tamaño.\n";
			break;
		}
		uint32_t nlen = 0;
		std::memcpy(&nlen, len4.data(), 4);
		uint32_t clen = ntohl(nlen);   // Convert from network to host byte order

		// 3) Receive ciphertext (clen bytes)
		auto cipher = m_net.ReceiveDataBinary(m_clientSock, static_cast<int>(clen));
		if (cipher.empty()) {
			std::cout << "[Server] Error al recibir datos.\n";
			break;
		}

		// 4) Decrypt and display message
		std::string plain = m_crypto.AESDecrypt(cipher, iv);
		std::cout << "\n[Cliente]: " << plain << "\nServidor: ";
		std::cout.flush();
	}
}

// Interactive loop for sending encrypted messages to the client
void
Server::SendEncryptedMessageLoop() {
	std::string msg;
	while (true) {
		std::cout << "Servidor: ";
		std::getline(std::cin, msg);
		if (msg == "/exit") break;

		std::vector<unsigned char> iv;
		auto cipher = m_crypto.AESEncrypt(msg, iv);

		// 1) Send IV (16 bytes)
		m_net.SendData(m_clientSock, iv);

		// 2) Send size in network byte order
		uint32_t clen = static_cast<uint32_t>(cipher.size());
		uint32_t nlen = htonl(clen);
		std::vector<unsigned char> len4(
			reinterpret_cast<unsigned char*>(&nlen),
			reinterpret_cast<unsigned char*>(&nlen) + 4
		);
		m_net.SendData(m_clientSock, len4);

		// 3) Send ciphertext
		m_net.SendData(m_clientSock, cipher);
	}
	std::cout << "[Server] Saliendo del chat.\n";
}

// Creates a bidirectional chat session with separate thread for receiving
void
Server::StartChatLoop() {
	std::thread recvThread([&]() {
		StartReceiveLoop();
		});

	SendEncryptedMessageLoop();

	if (recvThread.joinable())
		recvThread.join();
}