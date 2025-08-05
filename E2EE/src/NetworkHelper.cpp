#include "NetworkHelper.h"

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

NetworkHelper::~NetworkHelper() {
  if (m_serverSocket != INVALID_SOCKET) {
    closesocket(m_serverSocket);
  }

  if (m_initialized) {
    WSACleanup();
  }
}

bool
NetworkHelper::StartServer(int port) {
  // Crea el socket TCP
  m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (m_serverSocket == INVALID_SOCKET) {
    std::cerr << "Error creating socket: " << WSAGetLastError() << std::endl;
    return false;
  }

  // Configura la dirección del servidor (IPv4, cualquier IP local, puerto dado)
  sockaddr_in serverAddress{};
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(port);
  serverAddress.sin_addr.s_addr = INADDR_ANY;

  // Asocia el socket a la dirección y puerto
  if (bind(m_serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
    std::cerr << "Error binding socket: " << WSAGetLastError() << std::endl;
    closesocket(m_serverSocket);
    m_serverSocket = INVALID_SOCKET;
    return false;
  }

  // Escucha conexiones entrantes
  if (listen(m_serverSocket, SOMAXCONN) == SOCKET_ERROR) {
    std::cerr << "Error listening on socket: " << WSAGetLastError() << std::endl;
    closesocket(m_serverSocket);
    m_serverSocket = INVALID_SOCKET;
    return false;
  }

  std::cout << "Server started on port " << port << std::endl;
  return true;
}

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

bool
NetworkHelper::ConnectToServer(const std::string& ip, int port) {
  // Crea el socket TCP
  m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (m_serverSocket == INVALID_SOCKET) {
    std::cerr << "Error creating socket: " << WSAGetLastError() << std::endl;
    return false;
  }

  // Configura la dirección del servidor (IPv4, IP dada, puerto dado)
  sockaddr_in serverAddress{};
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(port);
  inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr);

  // Conecta al servidor
  if (connect(m_serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
    std::cerr << "Error connecting to server: " << WSAGetLastError() << std::endl;
    closesocket(m_serverSocket);
    m_serverSocket = INVALID_SOCKET;
    return false;
  }
  std::cout << "Connected to server at " << ip << ":" << port << std::endl;
  return true;
}

bool
NetworkHelper::SendData(SOCKET socket, const std::string& data) {
  return send(socket, data.c_str(), static_cast<int>(data.size()), 0) != SOCKET_ERROR;
}

bool
NetworkHelper::SendData(SOCKET socket, const std::vector<unsigned char>& data) {
  return send(socket,
    reinterpret_cast<const char*>(data.data()),
    static_cast<int>(data.size()), 0) != SOCKET_ERROR;
}

std::string
NetworkHelper::ReceiveData(SOCKET socket) {
  char buffer[4096] = {};
  int len = recv(socket, buffer, sizeof(buffer), 0);

  return std::string(buffer, len);
}

std::vector<unsigned char>
NetworkHelper::ReceiveData(SOCKET socket, int size) {
  std::vector<unsigned char> buffer(size);
  int len = recv(socket, reinterpret_cast<char*>(buffer.data()), size, 0);
  return buffer;
}

void
NetworkHelper::close(SOCKET socket) {
  closesocket(socket);
}