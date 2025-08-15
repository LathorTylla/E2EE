#include "NetworkHelper.h"

// Constructor initializes Windows Sockets (Winsock)
// Required for all network operations on Windows
NetworkHelper::NetworkHelper() : m_serverSocket(INVALID_SOCKET), m_initialized(false) {
  WSADATA wsaData;
  int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    std::cerr << "WSAStartup failed: " << result << std::endl;
  }
  else {
    m_initialized = true;
  }
}

// Destructor cleans up sockets and Winsock resources
NetworkHelper::~NetworkHelper() {
  if (m_serverSocket != INVALID_SOCKET) {
    closesocket(m_serverSocket);
  }

  if (m_initialized) {
    WSACleanup();
  }
}

// Creates a TCP server socket, binds it to the specified port,
// and starts listening for incoming connections
bool
NetworkHelper::StartServer(int port) {
  // Create TCP socket
  m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (m_serverSocket == INVALID_SOCKET) {
    std::cerr << "Error creating socket: " << WSAGetLastError() << std::endl;
    return false;
  }

  // Configure server address (IPv4, any local IP, given port)
  sockaddr_in serverAddress{};
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(port);
  serverAddress.sin_addr.s_addr = INADDR_ANY;

  // Bind socket to address and port
  if (bind(m_serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
    std::cerr << "Error binding socket: " << WSAGetLastError() << std::endl;
    closesocket(m_serverSocket);
    m_serverSocket = INVALID_SOCKET;
    return false;
  }

  // Listen for incoming connections
  if (listen(m_serverSocket, SOMAXCONN) == SOCKET_ERROR) {
    std::cerr << "Error listening on socket: " << WSAGetLastError() << std::endl;
    closesocket(m_serverSocket);
    m_serverSocket = INVALID_SOCKET;
    return false;
  }

  std::cout << "Server started on port " << port << std::endl;
  return true;
}

// Accepts an incoming client connection
// Blocks until a client connects
SOCKET
NetworkHelper::AcceptClient() {
  SOCKET clientSocket = accept(m_serverSocket, nullptr, nullptr);
  if (clientSocket == INVALID_SOCKET) {
    std::cerr << "Error accepting client: " << WSAGetLastError() << std::endl;
    return INVALID_SOCKET;
  }
  std::cout << "Client connected." << std::endl;
  return clientSocket;
}

// Connects to a remote server at the specified IP and port
bool
NetworkHelper::ConnectToServer(const std::string& ip, int port) {
  // Create TCP socket
  m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (m_serverSocket == INVALID_SOCKET) {
    std::cerr << "Error creating socket: " << WSAGetLastError() << std::endl;
    return false;
  }

  // Configure server address (IPv4, given IP, given port)
  sockaddr_in serverAddress{};
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(port);
  inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr);

  // Connect to server
  if (connect(m_serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
    std::cerr << "Error connecting to server: " << WSAGetLastError() << std::endl;
    closesocket(m_serverSocket);
    m_serverSocket = INVALID_SOCKET;
    return false;
  }
  std::cout << "Connected to server at " << ip << ":" << port << std::endl;
  return true;
}

// Sends string data over a socket
bool
NetworkHelper::SendData(SOCKET socket, const std::string& data) {
  return send(socket, data.c_str(), static_cast<int>(data.size()), 0) != SOCKET_ERROR;
}

// Sends binary data over a socket
// Uses SendAll to ensure all data is sent
bool
NetworkHelper::SendData(SOCKET socket, const std::vector<unsigned char>& data) {
  return SendAll(socket,
                 data.data(),
                 static_cast<int>(data.size()));
}

// Receives string data from a socket
// Note: Limited to 4KB of data
std::string
NetworkHelper::ReceiveData(SOCKET socket) {
  char buffer[4096] = {};
  int len = recv(socket, buffer, sizeof(buffer), 0);

  return std::string(buffer, len);
}

// Receives binary data of known size from a socket
// If size=0, returns empty vector on error
std::vector<unsigned char>
NetworkHelper::ReceiveDataBinary(SOCKET socket, int size) {
  std::vector<unsigned char> buf(size);
  if (!ReceiveExact(socket, buf.data(), size)) return {};
  return buf;
}

// Closes a socket connection
void
NetworkHelper::close(SOCKET socket) {
  closesocket(socket);
}

// Ensures all data is sent by handling partial sends
// Continues sending until all bytes are transmitted
bool
NetworkHelper::SendAll(SOCKET s, 
                       const unsigned char* data, 
                       int len) {
  int sent = 0;
  while (sent < len) {
    int n = send(s, (const char*)data + sent, len - sent, 0);
    if (n == SOCKET_ERROR) return false;
    sent += n;
  }
  return true;
}

// Ensures exactly the requested number of bytes are received
// Handles partial receives and continues until all data arrives
bool
NetworkHelper::ReceiveExact(SOCKET s, 
                            unsigned char* out, 
                            int len) {
  int recvd = 0;
  while (recvd < len) {
    int n = recv(s, (char*)out + recvd, len - recvd, 0);
    if (n <= 0) return false;
    recvd += n;
  }
  return true;
}