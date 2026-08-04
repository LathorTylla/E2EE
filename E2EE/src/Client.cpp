#include "Client.h"

namespace {
constexpr const char* kProtocolVersion = "E2EE/2";
constexpr const char* kProfilePrefix = "PROFILE:";
constexpr const char* kChatPrefix = "CHAT:";
constexpr const char* kTypingPrefix = "TYPING:";
}

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
	Disconnect();
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

bool
Client::PerformHandshake() {
	NotifyStatus("securing_session");
	const bool ready = ExchangeKeys() && SendAESKeyEncrypted();
	NotifyStatus(ready ? "secure" : "handshake_failed");
	return ready;
}

// Key exchange protocol:
// 1. Receive server's RSA public key
// 2. Send client's RSA public key to server
bool
Client::ExchangeKeys() {
	std::string protocol;
	if (!m_net.ReceiveFrame(m_serverSock, protocol, 32) || protocol != kProtocolVersion ||
		!m_net.SendFrame(m_serverSock, std::string(kProtocolVersion))) {
		std::cerr << "[Client] Version de protocolo incompatible.\n";
		return false;
	}
	// 1. Receive the server's public key
	std::string serverPubKey;
	if (!m_net.ReceiveFrame(m_serverSock, serverPubKey, 16 * 1024)) {
		std::cerr << "[Client] No se pudo recibir la clave publica del servidor.\n";
		return false;
	}
	m_crypto.LoadPeerPublicKey(serverPubKey);
	std::cout << "[Client] Clave pública del servidor recibida.\n";

	// 2. Send the client's public key
	std::string clientPubKey = m_crypto.GetPublicKeyString();
	if (!m_net.SendFrame(m_serverSock, clientPubKey)) {
		std::cerr << "[Client] No se pudo enviar la clave publica.\n";
		return false;
	}
	std::cout << "[Client] Clave pública del cliente enviada.\n";
	return true;
}

// Encrypt AES key with server's RSA public key and send it
// This is a crucial security step - only the server can decrypt this key
bool
Client::SendAESKeyEncrypted() {
	std::vector<unsigned char> encryptedAES = m_crypto.EncryptAESKeyWithPeer();
	if (!m_net.SendFrame(m_serverSock, encryptedAES)) {
		std::cerr << "[Client] No se pudo enviar la clave de sesion.\n";
		return false;
	}
	const auto profile = m_crypto.EncryptMessage(std::string(kProfilePrefix) + m_displayName);
	if (!m_net.SendFrame(m_serverSock, profile)) return false;
	std::vector<unsigned char> peerProfile;
	if (!m_net.ReceiveFrame(m_serverSock, peerProfile, 4096)) return false;
	std::string decodedProfile;
	if (!m_crypto.DecryptMessage(peerProfile, decodedProfile) ||
		decodedProfile.rfind(kProfilePrefix, 0) != 0) return false;
	m_peerName = decodedProfile.substr(std::strlen(kProfilePrefix));
	if (m_peerName.empty() || m_peerName.size() > 64) return false;
	m_safetyNumber = m_crypto.GetSessionSafetyNumber();
	std::cout << "[Client] Clave AES cifrada y enviada al servidor.\n";
	std::cout << "[Client] Codigo de seguridad: " << m_safetyNumber << "\n";
	return true;
}

// Application payloads are typed, encrypted with AES-256-GCM, then framed.
bool
Client::SendEncryptedMessage(const std::string& message) {
	if (m_serverSock == INVALID_SOCKET) return false;
	if (message.size() > 64 * 1024) {
		std::cerr << "[Client] El mensaje supera el limite de 64 KiB.\n";
		return false;
	}
	return m_net.SendFrame(m_serverSock, m_crypto.EncryptMessage(std::string(kChatPrefix) + message));
}

bool
Client::SendTypingNotification(bool typing) {
	if (m_serverSock == INVALID_SOCKET) return false;
	return m_net.SendFrame(
		m_serverSock,
		m_crypto.EncryptMessage(std::string(kTypingPrefix) + (typing ? "1" : "0"))
	);
}

bool
Client::StartReceiving() {
	if (m_serverSock == INVALID_SOCKET || m_running || m_rxThread.joinable()) return false;
	m_running = true;
	m_rxThread = std::thread([this]() { StartReceiveLoop(); });
	return true;
}

void
Client::Disconnect() {
	m_running = false;
	// Close before joining so a blocking recv() wakes immediately on Windows.
	m_net.close(m_serverSock);
	if (m_rxThread.joinable() && m_rxThread.get_id() != std::this_thread::get_id()) {
		m_rxThread.join();
	}
}

bool
Client::IsConnected() const {
	return m_serverSock != INVALID_SOCKET;
}

void
Client::SetMessageHandler(MessageHandler handler) {
	m_messageHandler = std::move(handler);
}

void
Client::SetStatusHandler(StatusHandler handler) {
	m_statusHandler = std::move(handler);
}

void
Client::SetTypingHandler(TypingHandler handler) {
	m_typingHandler = std::move(handler);
}

void Client::SetDisplayName(const std::string& name) {
	if (!name.empty() && name.size() <= 64) m_displayName = name;
}

const std::string& Client::GetPeerName() const { return m_peerName; }
const std::string& Client::GetSessionSafetyNumber() const { return m_safetyNumber; }

void
Client::NotifyStatus(const std::string& status) const {
	if (m_statusHandler) m_statusHandler(status);
}

// Interactive message loop - Get user input, encrypt and send
// Loop exits when user types "/exit"
void
Client::SendEncryptedMessageLoop() {
	std::string msg;
	while (m_running) {
		std::cout << "Cliente: ";
		std::getline(std::cin, msg);
		if (!std::cin || msg == "/exit") break;
		if (!SendEncryptedMessage(msg)) break;
	}
}

// Receives one authenticated frame at a time and dispatches its payload type.
void
Client::StartReceiveLoop() {
	while (m_running) {
		std::vector<unsigned char> packet;
		if (!m_net.ReceiveFrame(m_serverSock, packet, 64 * 1024 + 64)) {
			std::cout << "\n[Client] Conexión cerrada por el servidor.\n";
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
			std::cout << "\n[Servidor]: " << plain << "\nCliente: ";
			std::cout.flush();
		}
	}
	m_running = false;
	NotifyStatus("connection_closed");
	std::cout << "[Client] ReceiveLoop terminado.\n";
}

// Creates a bidirectional chat - receives messages in a separate thread
// while sending messages in the main thread
void 
Client::StartChatLoop() {
	if (!StartReceiving()) return;
	SendEncryptedMessageLoop();
	Disconnect();
}
