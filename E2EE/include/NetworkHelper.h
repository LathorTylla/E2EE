#pragma once
#include "Prerequisites.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

/**
 * @brief Helper class for network operations using Windows Sockets
 * 
 * This class provides a simplified interface for network communication
 * using Windows Sockets (Winsock). It handles socket initialization,
 * connection management, and data transmission for both client and server roles.
 * 
 * Data Flow:
 * 1. Initialize Winsock (done in constructor)
 * 2. For server: StartServer - AcceptClient - SendData/ReceiveData
 * 3. For client: ConnectToServer - SendData/ReceiveData
 * 4. Close sockets when done
 */
class
NetworkHelper {
public:
	/**
	 * @brief Constructor - Initializes Winsock
	 */
	NetworkHelper();
	
	/**
	 * @brief Destructor - Cleans up Winsock resources
	 */
	~NetworkHelper();

	/**
	 * @brief Starts a server socket that listens for incoming connections
	 * @param port The port number to listen on
	 * @return true if server started successfully, false otherwise
	 */
	bool
	StartServer(int port);

	/**
	 * @brief Accepts an incoming client connection
	 * @return Socket handle for the connected client, INVALID_SOCKET on failure
	 */
	SOCKET
	AcceptClient();

	/**
	 * @brief Connects to a remote server
	 * @param ip The IP address of the server
	 * @param port The port number of the server
	 * @return true if connection was successful, false otherwise
	 */
	bool
	ConnectToServer(const std::string& ip, int port);

	/**
	 * @brief Sends string data over a socket
	 * @param socket The socket to send data through
	 * @param data The string data to send
	 * @return true if data was sent successfully, false otherwise
	 */
	bool
	SendData(SOCKET socket, const std::string& data);

	/**
	 * @brief Sends binary data over a socket
	 * @param socket The socket to send data through
	 * @param data Vector of bytes to send
	 * @return true if data was sent successfully, false otherwise
	 */
	bool
	SendData(SOCKET socket, const std::vector<unsigned char>& data);

	/**
	 * @brief Receives string data from a socket
	 * @param socket The socket to receive data from
	 * @return The received string data
	 */
	std::string
	ReceiveData(SOCKET socket);

	/**
	 * @brief Receives binary data from a socket
	 * @param socket The socket to receive data from
	 * @param size Expected size of data (0 for unknown size)
	 * @return Vector of bytes containing the received data
	 */
	std::vector<unsigned char>
	ReceiveDataBinary(SOCKET socket, int size = 0);

	/**
	 * @brief Closes a socket connection
	 * @param socket The socket to close
	 */
	void
	close(SOCKET socket);

	/**
	 * @brief Ensures all data is sent over the socket
	 * @param s The socket to send data through
	 * @param data Pointer to the data buffer
	 * @param len Length of the data in bytes
	 * @return true if all data was sent successfully, false otherwise
	 */
	bool
	SendAll(SOCKET s, 
					const unsigned char* data, 
					int len);

	/**
	 * @brief Ensures exactly the requested number of bytes are received
	 * @param s The socket to receive data from
	 * @param out Pointer to the output buffer
	 * @param len Number of bytes to receive
	 * @return true if all requested bytes were received, false otherwise
	 */
	bool
	ReceiveExact(SOCKET s, 
							unsigned char* out, 
							int len);

public:
	SOCKET m_serverSocket = -1;  //< Socket handle for the server
	bool m_initialized;         //< Flag indicating if Winsock was initialized successfully
};