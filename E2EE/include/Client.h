#pragma once
#include "NetworkHelper.h"
#include "CryptoHelper.h"
#include "Prerequisites.h"

/**
 * @brief Client class for secure network communication
 * 
 * This class implements a client that can connect to a server,
 * exchange cryptographic keys, and send encrypted messages.
 * It uses RSA for key exchange and AES for message encryption.
 * 
 * Data Flow:
 * 1. Client connects to server
 * 2. Client and server exchange RSA public keys
 * 3. Client generates an AES key and encrypts it with the server's RSA public key
 * 4. Client sends the encrypted AES key to the server
 * 5. All subsequent messages are encrypted with the AES key
 */
class 
Client {
public:
    using MessageHandler = std::function<void(const std::string&)>;
    using StatusHandler = std::function<void(const std::string&)>;
    using TypingHandler = std::function<void(bool)>;
    /**
     * @brief Default constructor
     */
    Client() = default;
    
    /**
     * @brief Constructs a client with server address information
     * @param ip The IP address of the server
     * @param port The port number of the server
     */
    Client(const std::string& ip, int port);
    
    /**
     * @brief Destructor - Cleans up socket connections
     */
    ~Client();

    /**
     * @brief Connects to the server
     * @return true if connection was successful, false otherwise
     */
    bool
    Connect();

    /**
     * @brief Exchanges RSA public keys with the server
     * 
     * Generates RSA key pair, sends public key to server,
     * and receives server's public key.
     */
    bool
    ExchangeKeys();

    /**
     * @brief Sends the AES key encrypted with the server's RSA public key
     * 
     * Generates a new AES key, encrypts it with the server's
     * public key, and sends it to establish secure communication.
     */
    bool
    SendAESKeyEncrypted();

    /**
     * @brief Sends an encrypted message to the server
     * @param message The plaintext message to encrypt and send
     */
    bool
    SendEncryptedMessage(const std::string& message);

    bool SendTypingNotification(bool typing);

    bool
    PerformHandshake();

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
     * @brief Enters a loop for sending encrypted messages
     * 
     * Continuously prompts for user input and sends
     * the messages encrypted to the server.
     */
    void 
    SendEncryptedMessageLoop();

    /**
     * @brief Starts the chat loop for sending and receiving messages
     * 
     * Creates a separate thread for receiving messages while
     * allowing the user to send messages in the main thread.
     */
    void
    StartChatLoop();

    /**
     * @brief Starts a loop for receiving messages from the server
     * 
     * Continuously listens for incoming encrypted messages,
     * decrypts them, and displays them to the user.
     */
    void
    StartReceiveLoop();     

private:
    std::string m_ip;        ///< Server IP address
    int m_port{ 0 };         ///< Server port number
    SOCKET m_serverSock{ INVALID_SOCKET }; ///< Socket for server connection
    NetworkHelper m_net;     ///< Helper for network operations
    CryptoHelper m_crypto;   ///< Helper for cryptographic operations
    std::atomic<bool> m_running{ false };
    std::thread m_rxThread;
    MessageHandler m_messageHandler;
    StatusHandler m_statusHandler;
    TypingHandler m_typingHandler;
    std::string m_displayName{ "Cliente" };
    std::string m_peerName{ "Servidor" };
    std::string m_safetyNumber;

    void NotifyStatus(const std::string& status) const;
};
