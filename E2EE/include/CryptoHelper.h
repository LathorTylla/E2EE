#pragma once

#include "Prerequisites.h"
#include <openssl/evp.h>

class CryptoHelper {
public:
  static constexpr size_t AesKeySize = 32;
  static constexpr size_t GcmNonceSize = 12;
  static constexpr size_t GcmTagSize = 16;
  static constexpr unsigned char MessageVersion = 1;

  CryptoHelper();
  ~CryptoHelper();

  CryptoHelper(const CryptoHelper&) = delete;
  CryptoHelper& operator=(const CryptoHelper&) = delete;

  void GenerateRSAKeys();
  std::string GetPublicKeyString() const;
  void LoadPeerPublicKey(const std::string& pemKey);

  void GenerateAESKey();
  std::vector<unsigned char> EncryptAESKeyWithPeer() const;
  void DecryptAESKey(const std::vector<unsigned char>& encryptedKey);

  std::vector<unsigned char> EncryptMessage(const std::string& plaintext) const;
  bool DecryptMessage(const std::vector<unsigned char>& packet,
                      std::string& plaintext) const;

  std::string GetOwnKeyFingerprint() const;
  std::string GetPeerKeyFingerprint() const;
  std::string GetSessionSafetyNumber() const;

private:
  EVP_PKEY* m_keyPair{ nullptr };
  EVP_PKEY* m_peerPublicKey{ nullptr };
  unsigned char m_aesKey[AesKeySize]{};
  bool m_hasAESKey{ false };

  static std::vector<unsigned char> PublicKeyDer(EVP_PKEY* key);
  static std::string Fingerprint(const std::vector<unsigned char>& bytes,
                                 size_t groups = 8);
  static void Require(bool result, const char* operation);
};
