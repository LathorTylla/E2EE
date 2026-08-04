#include "Server.h"

namespace {
constexpr const char* kProtocolVersion = "E2EE/2";
constexpr const char* kProfilePrefix = "PROFILE:";
constexpr const char* kChatPrefix = "CHAT:";
constexpr const char* kTypingPrefix = "TYPING:";
}

// Constructor initializes the server with a port and generates RSA keys
Server::Server(int port) : m_port(port), m_clientSock(INVALID_SOCKET) {
	// Generate RSA keys on construction
	m_crypto.GenerateRSAKeys();
}

// Destructor cleans up client socket connection if active
Server::~Server() {
	Disconnect();
}

// Starts the server listening on the specified port
bool 
Server::Start() {
	std::cout << "[Server] Iniciando servidor en el puerto " << m_port << "...\n";
	const bool started = m_net.StartServer(m_port);
	NotifyStatus(started ? "listening" : "listen_failed");
	return started;
}

// Waits for a client connection and performs the secure key exchange
bool
Server::WaitForClient() {
	std::cout << "[Server] Esperando conexión de un cliente...\n";

	// Accept incoming connection
	m_clientSock = m_net.AcceptClient();
	if (m_clientSock == INVALID_SOCKET) {
		std::cerr << "[Server] No se pudo aceptar cliente.\n";
		return false;
	}
	std::cout << "[Server] Cliente conectado.\n";
	NotifyStatus("client_connected");
	if (!m_net.SendFrame(m_clientSock, std::string(kProtocolVersion))) return false;
	std::string protocol;
	if (!m_net.ReceiveFrame(m_clientSock, protocol, 32) || protocol != kProtocolVersion) {
		return false;
	}

	// Key exchange protocol:
	// 1. Send server's public key to client
	std::string serverPubKey = m_crypto.GetPublicKeyString();
	if (!m_net.SendFrame(m_clientSock, serverPubKey)) return false;

	// 2. Receive client's public key
	std::string clientPubKey;
	if (!m_net.ReceiveFrame(m_clientSock, clientPubKey, 16 * 1024)) return false;
	m_crypto.LoadPeerPublicKey(clientPubKey);

	// 3. Receive AES key encrypted with server's public key
	std::vector<unsigned char> encryptedAESKey;
	if (!m_net.ReceiveFrame(m_clientSock, encryptedAESKey, 512) || encryptedAESKey.empty()) {
		return false;
	}
	m_crypto.DecryptAESKey(encryptedAESKey);
	std::vector<unsigned char> peerProfile;
	if (!m_net.ReceiveFrame(m_clientSock, peerProfile, 4096)) return false;
	std::string decodedProfile;
	if (!m_crypto.DecryptMessage(peerProfile, decodedProfile) ||
		decodedProfile.rfind(kProfilePrefix, 0) != 0) return false;
	m_peerName = decodedProfile.substr(std::strlen(kProfilePrefix));
	if (m_peerName.empty() || m_peerName.size() > 64) return false;
	const auto profile = m_crypto.EncryptMessage(std::string(kProfilePrefix) + m_displayName);
	if (!m_net.SendFrame(m_clientSock, profile)) return false;
	m_safetyNumber = m_crypto.GetSessionSafetyNumber();

	std::cout << "[Server] Clave AES intercambiada exitosamente.\n";
	std::cout << "[Server] Codigo de seguridad: " << m_safetyNumber << "\n";
	NotifyStatus("secure");
	return true;
}

bool
Server::SendEncryptedMessage(const std::string& message) {
	if (m_clientSock == INVALID_SOCKET || message.size() > 64 * 1024) return false;
	return m_net.SendFrame(m_clientSock, m_crypto.EncryptMessage(std::string(kChatPrefix) + message));
}

bool
Server::SendTypingNotification(bool typing) {
	if (m_clientSock == INVALID_SOCKET) return false;
	return m_net.SendFrame(
		m_clientSock,
		m_crypto.EncryptMessage(std::string(kTypingPrefix) + (typing ? "1" : "0"))
	);
}

bool
Server::StartReceiving() {
	if (m_clientSock == INVALID_SOCKET || m_running || m_rxThread.joinable()) return false;
	m_running = true;
	m_rxThread = std::thread([this]() { StartReceiveLoop(); });
	return true;
}

void
Server::Disconnect() {
	m_running = false;
	m_net.close(m_net.m_serverSocket);
	// Close before joining so a blocking recv() wakes immediately on Windows.
	m_net.close(m_clientSock);
	if (m_rxThread.joinable() && m_rxThread.get_id() != std::this_thread::get_id()) {
		m_rxThread.join();
	}
}

bool
Server::IsConnected() const {
	return m_clientSock != INVALID_SOCKET;
}

void
Server::SetMessageHandler(MessageHandler handler) {
	m_messageHandler = std::move(handler);
}

void
Server::SetStatusHandler(StatusHandler handler) {
	m_statusHandler = std::move(handler);
}

void
Server::SetTypingHandler(TypingHandler handler) {
	m_typingHandler = std::move(handler);
}

void Server::SetDisplayName(const std::string& name) {
	if (!name.empty() && name.size() <= 64) m_displayName = name;
}

const std::string& Server::GetPeerName() const { return m_peerName; }
const std::string& Server::GetSessionSafetyNumber() const { return m_safetyNumber; }

void
Server::NotifyStatus(const std::string& status) const {
	if (m_statusHandler) m_statusHandler(status);
}

// Receives and decrypts a single message from the client
void
Server::ReceiveEncryptedMessage() {
	std::vector<unsigned char> packet;
	if (!m_net.ReceiveFrame(m_clientSock, packet, 64 * 1024 + 64)) {
		return;
	}
	std::string msg;
	if (!m_crypto.DecryptMessage(packet, msg)) return;
	if (msg.rfind(kChatPrefix, 0) != 0) return;
	msg.erase(0, std::strlen(kChatPrefix));
	std::cout << "[Server] Mensaje recibido: " << msg << "\n";
}

// Continuously receives encrypted messages from the client
void
Server::StartReceiveLoop() {
	while (m_running) {
		std::vector<unsigned char> packet;
		if (!m_net.ReceiveFrame(m_clientSock, packet, 64 * 1024 + 64)) {
			std::cout << "\n[Server] Conexión cerrada por el cliente.\n";
			break;
		}
		std::string plain;
		if (!m_crypto.DecryptMessage(packet, plain)) {
			NotifyStatus("authentication_failed");
			break;
		}
		if (plain.rfind(kTypingPrefix, 0) == 0) {
			if (m_typingHandler) m_typingHandler(plain == std::string(kTypingPrefix) + "1");
			continue;
		}
		if (plain.rfind(kChatPrefix, 0) != 0) {
			NotifyStatus("authentication_failed");
			break;
		}
		plain.erase(0, std::strlen(kChatPrefix));
		if (m_messageHandler) m_messageHandler(plain);
		else {
			std::cout << "\n[Cliente]: " << plain << "\nServidor: ";
			std::cout.flush();
		}
	}
	m_running = false;
	NotifyStatus("connection_closed");
}

// Interactive loop for sending encrypted messages to the client
void
Server::SendEncryptedMessageLoop() {
	std::string msg;
	while (m_running) {
		std::cout << "Servidor: ";
		std::getline(std::cin, msg);
		if (!std::cin || msg == "/exit") break;

		if (!SendEncryptedMessage(msg)) break;
	}
	std::cout << "[Server] Saliendo del chat.\n";
}

// Creates a bidirectional chat session with separate thread for receiving
void
Server::StartChatLoop() {
	if (!StartReceiving()) return;
	SendEncryptedMessageLoop();
	Disconnect();
}
