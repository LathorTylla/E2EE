#include "Prerequisites.h"
#include "Server.h"
#include "Client.h"

// Initializes and runs the server component
// - Creates a server instance on the specified port
// - Waits for a client connection
// - Performs secure key exchange
// - Starts encrypted bidirectional chat
static 
void 
runServer(int port) {
  Server s(port);
  if (!s.Start()) {
    std::cerr << "[Main] No se pudo iniciar el servidor.\n";
    return;
  }
  s.WaitForClient(); 
  s.StartChatLoop(); 
}

// Initializes and runs the client component
// - Connects to a server at the specified IP and port
// - Performs secure key exchange (RSA and AES)
// - Starts encrypted bidirectional chat
static 
void 
runClient(const std::string& ip, int port) {
  Client c(ip, port);
  if (!c.Connect()) { std::cerr << "No se pudo conectar.\n"; return; }

  c.ExchangeKeys();          
  c.SendAESKeyEncrypted();  

  c.StartChatLoop();
}

// Main entry point - handles command-line arguments or interactive input
// Supports two modes of operation:
// 1. Server mode: E2EE server [port]
// 2. Client mode: E2EE client <ip> <port>
int 
main(int argc, char** argv) {
  std::string mode, ip;
  int port = 0;

  // Command-line argument processing
  if (argc >= 2) {
    mode = argv[1];
    if (mode == "server") {
      port = (argc >= 3) ? std::stoi(argv[2]) : 12345;
    }
    else if (mode == "client") {
      if (argc < 4) { std::cerr << "Uso: E2EE client <ip> <port>\n"; return 1; }
      ip = argv[2];
      port = std::stoi(argv[3]);
    }
    else {
      std::cerr << "Modo no reconocido. Usa: server | client\n";
      return 1;
    }
  }
  // Interactive mode for user input
  else {
    std::cout << "Modo (server/client): ";
    std::cin >> mode;
    if (mode == "server") {
      std::cout << "Puerto: ";
      std::cin >> port;
    }
    else if (mode == "client") {
      std::cout << "IP: ";
      std::cin >> ip;
      std::cout << "Puerto: ";
      std::cin >> port;
    }
    else {
      std::cerr << "Modo no reconocido.\n";
      return 1;
    }
    // Clear input buffer
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  }
  // Launch appropriate mode
  if (mode == "server") runServer(port);
  else runClient(ip, port);
  return 0;
}