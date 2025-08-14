#include "Prerequisites.h"
#include "Server.h"
#include "Client.h"

static 
void 
runServer(int port) {
  Server s(port);
  if (!s.Start()) { std::cerr << " No se pudo iniciar.\n"; return; }
  s.WaitForClient(); 
  s.StartReceiveLoop();       
}

static void runClient(const std::string& ip, int port) {
  Client c(ip, port);
  if (!c.Connect()) { std::cerr << "No se pudo conectar.\n"; return; }

  c.ExchangeKeys();          
  c.SendAESKeyEncrypted();  

  c.SendEncryptedMessageLoop();
}

int 
main(int argc, char** argv) {
  std::string mode, ip;
  int port = 0;

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
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  }

  if (mode == "server") runServer(port);
  else runClient(ip, port);
  return 0;
}