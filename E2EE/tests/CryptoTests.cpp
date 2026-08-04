#include "CryptoHelper.h"
#include "Client.h"
#include "Server.h"

#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

void EstablishSession(CryptoHelper& alice, CryptoHelper& bob) {
  alice.GenerateRSAKeys();
  bob.GenerateRSAKeys();
  alice.LoadPeerPublicKey(bob.GetPublicKeyString());
  bob.LoadPeerPublicKey(alice.GetPublicKeyString());
  alice.GenerateAESKey();
  bob.DecryptAESKey(alice.EncryptAESKeyWithPeer());
}

void TestRoundTrip() {
  CryptoHelper alice;
  CryptoHelper bob;
  EstablishSession(alice, bob);

  const std::vector<std::string> samples{
    "",
    "Hola, mundo",
    u8"Cifrado con acentos: áéíóú y 🔐",
    std::string(32 * 1024, 'E')
  };

  for (const auto& sample : samples) {
    std::string plain;
    Expect(bob.DecryptMessage(alice.EncryptMessage(sample), plain),
           "No se pudo autenticar un mensaje válido");
    Expect(plain == sample, "El mensaje descifrado cambió");
  }
}

void TestTamperingIsRejected() {
  CryptoHelper alice;
  CryptoHelper bob;
  EstablishSession(alice, bob);

  auto packet = alice.EncryptMessage("mensaje íntegro");
  packet.back() ^= 0x01;

  std::string plain = "no debe sobrevivir";
  Expect(!bob.DecryptMessage(packet, plain),
         "AES-GCM aceptó un paquete alterado");
  Expect(plain.empty(), "Se expuso texto tras fallar la autenticación");
}

void TestSessionIdentity() {
  CryptoHelper alice;
  CryptoHelper bob;
  EstablishSession(alice, bob);

  Expect(!alice.GetOwnKeyFingerprint().empty(), "Falta fingerprint local");
  Expect(!alice.GetPeerKeyFingerprint().empty(), "Falta fingerprint remoto");
  Expect(alice.GetSessionSafetyNumber() == bob.GetSessionSafetyNumber(),
         "Los códigos de seguridad no coinciden");
}

void TestLoopbackProtocol() {
  constexpr int port = 45931;
  Server server(port);
  Client client("127.0.0.1", port);
  server.SetDisplayName("Servidor Test");
  client.SetDisplayName("Cliente Test");

  std::promise<std::string> serverMessage;
  std::promise<std::string> clientMessage;
  std::promise<bool> serverTyping;
  server.SetMessageHandler([&](const std::string& value) {
    serverMessage.set_value(value);
  });
  client.SetMessageHandler([&](const std::string& value) {
    clientMessage.set_value(value);
  });
  server.SetTypingHandler([&](bool value) {
    if (value) serverTyping.set_value(true);
  });

  std::promise<bool> listening;
  std::promise<bool> accepted;
  auto listeningFuture = listening.get_future();
  auto acceptedFuture = accepted.get_future();
  std::thread serverSetup([&]() {
    const bool started = server.Start();
    listening.set_value(started);
    accepted.set_value(started && server.WaitForClient() && server.StartReceiving());
  });

  Expect(listeningFuture.get(), "El servidor local no pudo escuchar");
  Expect(client.Connect(), "El cliente local no pudo conectar");
  Expect(client.PerformHandshake(), "Falló el handshake local");
  Expect(client.StartReceiving(), "No inició la recepción del cliente");
  const bool serverAccepted = acceptedFuture.get();
  serverSetup.join();
  Expect(serverAccepted, "El servidor no completó el handshake");

  Expect(client.GetPeerName() == "Servidor Test", "Perfil remoto incorrecto");
  Expect(server.GetPeerName() == "Cliente Test", "Perfil del cliente incorrecto");
  Expect(client.GetSessionSafetyNumber() == server.GetSessionSafetyNumber(),
         "La identidad de la sesión local no coincide");

  auto serverMessageFuture = serverMessage.get_future();
  auto clientMessageFuture = clientMessage.get_future();
  auto typingFuture = serverTyping.get_future();
  Expect(client.SendEncryptedMessage(u8"ping seguro 🔐"), "No se envió client → server");
  Expect(serverMessageFuture.wait_for(std::chrono::seconds(3)) == std::future_status::ready &&
         serverMessageFuture.get() == u8"ping seguro 🔐", "Falló client → server");
  Expect(server.SendEncryptedMessage("pong seguro"), "No se envió server → client");
  Expect(clientMessageFuture.wait_for(std::chrono::seconds(3)) == std::future_status::ready &&
         clientMessageFuture.get() == "pong seguro", "Falló server → client");
  Expect(client.SendTypingNotification(true), "No se envió el estado de escritura");
  Expect(typingFuture.wait_for(std::chrono::seconds(3)) == std::future_status::ready,
         "No se recibió el estado de escritura");

  client.Disconnect();
  server.Disconnect();
}

}  // namespace

int main() {
  try {
    TestRoundTrip();
    TestTamperingIsRejected();
    TestSessionIdentity();
    TestLoopbackProtocol();
    std::cout << "[OK] 4 suites: criptografia e integracion TCP completadas.\n";
    return 0;
  }
  catch (const std::exception& error) {
    std::cerr << "[ERROR] " << error.what() << "\n";
    return 1;
  }
}
