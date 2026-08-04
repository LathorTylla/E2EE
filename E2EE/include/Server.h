#pragma once
#include "NetworkHelper.h"
#include "CryptoHelper.h"
#include "Prerequisites.h"

/**
 * @brief Server class for secure network communication
 * 
 * This class implements a server that can accept client connections,
 * exchange cryptographic keys, and communicate securely using
 * encryption. It handles the server-side of the secure communication protocol.
 * 
 * Data Flow:
 * 1. Server starts and listens for connections
 * 2. Server accepts a client connection
 * 3. Server and client exchange RSA public keys
 * 4. Server receives the AES key encrypted with its RSA public key
 * 5. Server decrypts the AES key using its RSA private key
 * 6. All subsequent messages are encrypted with the AES key
 */
class
Server {
public:
	using MessageHandler = std::function<void(const std::string&)>;
	using StatusHandler = std::function<void(const std::string&)>;
	using TypingHandler = std::function<void(bool)>;
	/**
	 * @brief Default constructor
	 */
	Server() = default;
	
	/**
	 * @brief Constructs a server that listens on a specific port
	 * @param port The port number to listen on
	 */
	Server(int port);

	/**
	 * @brief Destructor - Cleans up resources and stops threads
	 */
	~Server();

	/**
	 * @brief Starts the server and begins listening for connections
	 * @return true if server started successfully, false otherwise
	 */
	bool 
	Start();

	/**
	 * @brief Waits for a client to connect and handles key exchange
	 * 
	 * This method blocks until a client connects, then performs
	 * the initial handshake and cryptographic key exchange.
	 */
	bool
  WaitForClient();

	/**
	 * @brief Receives and processes an encrypted message from the client
	 * 
	 * Receives encrypted data, decrypts it using the AES key,
	 * and displays the message.
	 */
	void
	ReceiveEncryptedMessage();

	bool
	SendEncryptedMessage(const std::string& message);

	bool SendTypingNotification(bool typing);

	bool
	StartReceiving();

	void
	Disconnect();

	bool
	IsConnected() const;

	void
	SetMessageHandler(MessageHandler handler);

	void
	SetStatusHandler(StatusHandler handler);

	void SetTypingHandler(TypingHandler handler);

	void SetDisplayName(const std::string& name);
	const std::string& GetPeerName() const;
	const std::string& GetSessionSafetyNumber() const;

	/**
	 * @brief Starts a continuous loop for receiving encrypted messages
	 * 
	 * Creates a thread that continuously listens for and processes
	 * incoming encrypted messages from the client.
	 */
	void
	StartReceiveLoop();

	/**
	 * @brief Enters a loop for sending encrypted messages to the client
	 * 
	 * Continuously prompts for user input and sends the
	 * messages encrypted to the connected client.
	 */
	void
	SendEncryptedMessageLoop();

	/**
	 * @brief Starts the chat loop for both sending and receiving messages
	 * 
	 * Creates a separate thread for receiving messages while
	 * allowing the user to send messages in the main thread.
	 */
	void
	StartChatLoop();

private:
	int m_port{ 0 };                    ///< Port number to listen on
	SOCKET m_clientSock{ INVALID_SOCKET }; ///< Socket for the connected client
	NetworkHelper m_net;                ///< Helper for network operations
	CryptoHelper m_crypto;              ///< Helper for cryptographic operations
	std::thread m_rxThread;             ///< Thread for receiving messages
	std::atomic<bool> m_running{ false }; ///< Flag indicating if the server is running
	MessageHandler m_messageHandler;
	StatusHandler m_statusHandler;
	TypingHandler m_typingHandler;
	std::string m_displayName{ "Servidor" };
	std::string m_peerName{ "Cliente" };
	std::string m_safetyNumber;

	void NotifyStatus(const std::string& status) const;
};
