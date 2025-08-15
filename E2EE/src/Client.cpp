#include "Client.h"

// Constructor initializes client with server connection info and generates cryptographic keys
Client::Client(const std::string& ip, 
							 int port)
	: m_ip(ip), m_port(port), m_serverSock(INVALID_SOCKET) {
	// Generate RSA key pair when instantiated
	m_crypto.GenerateRSAKeys();
	// Generate the AES key that will be used for message encryption
	m_crypto.GenerateAESKey();
}

// Destructor cleans up network resources
Client::~Client() {
	if (m_serverSock != INVALID_SOCKET) {
		m_net.close(m_serverSock);
	}
}

// Establishes connection with the server using provided IP and port
bool
Client::Connect() {
	std::cout << "[Client] Conectando al servidor " << m_ip << ":" << m_port << "...\n";
	bool connected = m_net.ConnectToServer(m_ip, m_port);
	if (connected) {
		m_serverSock = m_net.m_serverSocket; // Store the socket once connected
		std::cout << "[Client] Conexión establecida.\n";
	}
	else {
		std::cerr << "[Client] Error al conectar.\n";
	}
	return connected;
}

// Key exchange protocol:
// 1. Receive server's RSA public key
// 2. Send client's RSA public key to server
void
Client::ExchangeKeys() {
	// 1. Receive the server's public key
	std::string serverPubKey = m_net.ReceiveData(m_serverSock);
	m_crypto.LoadPeerPublicKey(serverPubKey);
	std::cout << "[Client] Clave pública del servidor recibida.\n";

	// 2. Send the client's public key
	std::string clientPubKey = m_crypto.GetPublicKeyString();
	m_net.SendData(m_serverSock, clientPubKey);
	std::cout << "[Client] Clave pública del cliente enviada.\n";
}

// Encrypt AES key with server's RSA public key and send it
// This is a crucial security step - only the server can decrypt this key
void
Client::SendAESKeyEncrypted() {
	std::vector<unsigned char> encryptedAES = m_crypto.EncryptAESKeyWithPeer();
	m_net.SendData(m_serverSock, encryptedAES);
	std::cout << "[Client] Clave AES cifrada y enviada al servidor.\n";
}

// Encrypted message protocol:
// 1. Send IV (16 bytes)
// 2. Send encrypted message length (4 bytes, network byte order)
// 3. Send encrypted message
void
Client::SendEncryptedMessage(const std::string& message) {
	std::vector<unsigned char> iv;
	auto cipher = m_crypto.AESEncrypt(message, iv);

	// 1) Send IV
	m_net.SendData(m_serverSock, iv);

	// 2) Send size (uint32_t) in network byte order
	uint32_t clen = static_cast<uint32_t>(cipher.size());
	uint32_t nlen = htonl(clen);
	std::vector<unsigned char> len4(reinterpret_cast<unsigned char*>(&nlen),
		reinterpret_cast<unsigned char*>(&nlen) + 4);
	m_net.SendData(m_serverSock, len4);

	// 3) Send ciphertext
	m_net.SendData(m_serverSock, cipher);
}

// Interactive message loop - Get user input, encrypt and send
// Loop exits when user types "/exit"
void
Client::SendEncryptedMessageLoop() {
	std::string msg;
	while (true) {
		std::cout << "Cliente: ";
		std::getline(std::cin, msg);
		if (msg == "/exit") break;

		std::vector<unsigned char> iv;
		auto cipher = m_crypto.AESEncrypt(msg, iv);

		m_net.SendData(m_serverSock, iv);

		uint32_t clen = static_cast<uint32_t>(cipher.size());
		uint32_t nlen = htonl(clen);
		std::vector<unsigned char> len4(reinterpret_cast<unsigned char*>(&nlen),
			reinterpret_cast<unsigned char*>(&nlen) + 4);
		m_net.SendData(m_serverSock, len4);

		m_net.SendData(m_serverSock, cipher);
	}
}

// Message receiving protocol:
// 1. Receive IV (16 bytes)
// 2. Receive message length (4 bytes)
// 3. Receive encrypted message
// 4. Decrypt and display
void
Client::StartReceiveLoop() {
	while (true) {
		// 1) IV (16 bytes)
		auto iv = m_net.ReceiveDataBinary(m_serverSock, 16);
		if (iv.empty()) {
			std::cout << "\n[Client] Conexión cerrada por el servidor.\n";
			break;
		}

		// 2) Size (4 bytes, network/big-endian)
		auto len4 = m_net.ReceiveDataBinary(m_serverSock, 4);
		if (len4.size() != 4) {
			std::cout << "[Client] Error al recibir tamaño.\n";
			break;
		}
		uint32_t nlen = 0;
		std::memcpy(&nlen, len4.data(), 4);
		uint32_t clen = ntohl(nlen);

		// 3) Ciphertext (clen bytes)
		auto cipher = m_net.ReceiveDataBinary(m_serverSock, static_cast<int>(clen));
		if (cipher.empty()) {
			std::cout << "[Client] Error al recibir datos.\n";
			break;
		}

		// 4) Decrypt and display
		std::string plain = m_crypto.AESDecrypt(cipher, iv);
		std::cout << "\n[Servidor]: " << plain << "\nCliente: ";
		std::cout.flush();
	}
	std::cout << "[Client] ReceiveLoop terminado.\n";
}

// Creates a bidirectional chat - receives messages in a separate thread
// while sending messages in the main thread
void 
Client::StartChatLoop() {
	std::thread recvThread([&]() {
		StartReceiveLoop();
		});

	SendEncryptedMessageLoop();

	if (recvThread.joinable())
		recvThread.join();
}
