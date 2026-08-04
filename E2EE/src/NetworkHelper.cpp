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
  if (!m_initialized || port < 1 || port > 65535) {
    std::cerr << "Invalid server configuration.\n";
    return false;
  }
  // Create TCP socket
  m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (m_serverSocket == INVALID_SOCKET) {
    std::cerr << "Error creating socket: " << WSAGetLastError() << std::endl;
    return false;
  }

  // Configure server address (IPv4, any local IP, given port)
  sockaddr_in serverAddress{};
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(static_cast<u_short>(port));
  serverAddress.sin_addr.s_addr = INADDR_ANY;

  BOOL reuseAddress = TRUE;
  setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char*>(&reuseAddress), sizeof(reuseAddress));

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
  if (!m_initialized || port < 1 || port > 65535) {
    std::cerr << "Invalid client configuration.\n";
    return false;
  }
  // Create TCP socket
  m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (m_serverSocket == INVALID_SOCKET) {
    std::cerr << "Error creating socket: " << WSAGetLastError() << std::endl;
    return false;
  }

  // Configure server address (IPv4, given IP, given port)
  sockaddr_in serverAddress{};
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(static_cast<u_short>(port));
  if (inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr) != 1) {
    std::cerr << "Invalid IPv4 address: " << ip << std::endl;
    closesocket(m_serverSocket);
    m_serverSocket = INVALID_SOCKET;
    return false;
  }

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
  return data.size() <= INT_MAX && SendAll(socket,
    reinterpret_cast<const unsigned char*>(data.data()), static_cast<int>(data.size()));
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

  return len > 0 ? std::string(buffer, len) : std::string{};
}

// Receives binary data of known size from a socket
// If size=0, returns empty vector on error
std::vector<unsigned char>
NetworkHelper::ReceiveDataBinary(SOCKET socket, int size) {
  if (size <= 0) return {};
  std::vector<unsigned char> buf(size);
  if (!ReceiveExact(socket, buf.data(), size)) return {};
  return buf;
}

// Closes a socket connection
bool
NetworkHelper::SendFrame(SOCKET socket, const std::vector<unsigned char>& data) {
  if (data.size() > MaxFrameSize) return false;
  const uint32_t networkSize = htonl(static_cast<uint32_t>(data.size()));
  return SendAll(socket, reinterpret_cast<const unsigned char*>(&networkSize), sizeof(networkSize)) &&
         (data.empty() || SendAll(socket, data.data(), static_cast<int>(data.size())));
}

bool
NetworkHelper::SendFrame(SOCKET socket, const std::string& data) {
  return SendFrame(socket, std::vector<unsigned char>(data.begin(), data.end()));
}

bool
NetworkHelper::ReceiveFrame(SOCKET socket, std::vector<unsigned char>& data, uint32_t maxSize) {
  uint32_t networkSize = 0;
  if (!ReceiveExact(socket, reinterpret_cast<unsigned char*>(&networkSize), sizeof(networkSize))) {
    return false;
  }
  const uint32_t size = ntohl(networkSize);
  if (size > maxSize || size > MaxFrameSize) {
    std::cerr << "Rejected oversized network frame: " << size << " bytes.\n";
    return false;
  }
  data.resize(size);
  return size == 0 || ReceiveExact(socket, data.data(), static_cast<int>(size));
}

bool
NetworkHelper::ReceiveFrame(SOCKET socket, std::string& data, uint32_t maxSize) {
  std::vector<unsigned char> bytes;
  if (!ReceiveFrame(socket, bytes, maxSize)) return false;
  data.assign(bytes.begin(), bytes.end());
  return true;
}

void
NetworkHelper::close(SOCKET& socket) {
  if (socket == INVALID_SOCKET) return;
  shutdown(socket, SD_BOTH);
  closesocket(socket);
  if (socket == m_serverSocket) m_serverSocket = INVALID_SOCKET;
  socket = INVALID_SOCKET;
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
    if (n == SOCKET_ERROR || n == 0) return false;
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
