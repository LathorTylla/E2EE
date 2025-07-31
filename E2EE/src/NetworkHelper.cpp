#include "NetworkHelper.h"

NetworkHelper::NetworkHelper() : m_serverSocket(INVALID_SOCKET), m_initialized(false) {
  WSAData wsaData;
  int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
   std::cerr << "WSAStartup failed with error: " << result << std::endl;
   return;
  } else {
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

bool NetworkHelper::StartServer(int port) {
  m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (m_serverSocket == INVALID_SOCKET) {
    std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
    return false;
  }

  //Configurar la dirección del servidor
  sockaddr_in serverAddress{};
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(port); // Convertir el puerto a formato de red
  serverAddress.sin_addr.s_addr = INADDR_ANY; // Aceptar conexiones de cualquier dirección IP

  //Associar el socket con la dirección del servidor
  if (bind(m_serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR) {
    std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
    closesocket(m_serverSocket);
    m_serverSocket = INVALID_SOCKET;
    return false;
  }

  // Escuchar conexiones entrantes
  if (listen(m_serverSocket, SOMAXCONN) == SOCKET_ERROR) {
    std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
    closesocket(m_serverSocket);
    m_serverSocket = INVALID_SOCKET;
    return false;
  }

  std::cout << "Server started on port " << port << std::endl;
  return true;
}

SOCKET NetworkHelper::ConnectToServer(const std::string& ip, int port) {
  //Crear un socket para el cliente
  m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if( m_serverSocket == INVALID_SOCKET) {
    std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
    return false;
  }

  //configurar la dirección del servidor
  sockaddr_in serverAddress{};
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(port); // Convertir el puerto a formato de red
  inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr); // Convertir la IP a formato binario

  // Conectar al servidor
  if (connect(m_serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR) {
    std::cerr << "Connect failed with error: " << WSAGetLastError() << std::endl;
    closesocket(m_serverSocket);
    m_serverSocket = INVALID_SOCKET;
    return false;
  }

  std::cout << "Connected to server at " << ip << ":" << port << std::endl;
  return true;
}