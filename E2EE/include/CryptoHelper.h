#pragma once
#include "Prerequisites.h"
#include "openssl\rsa.h"
#include "openssl\aes.h"

/**
 * @brief Helper class for cryptographic operations
 * 
 * This class provides cryptographic functionality for secure communication,
 * including RSA key generation for key exchange and AES encryption for
 * message security. It uses OpenSSL library for implementation.
 * 
 * Data Flow:
 * 1. Generate RSA key pair (public/private keys)
 * 2. Exchange public keys with peer
 * 3. Generate AES key for symmetric encryption
 * 4. Encrypt AES key with peer's RSA public key
 * 5. Use AES key for efficient message encryption/decryption
 */
class
CryptoHelper {
public:
 /**
  * @brief Default constructor
  */
 CryptoHelper();
 
 /**
  * @brief Destructor - Cleans up RSA key structures
  */
 ~CryptoHelper();

  // RSA
  /**
   * @brief Generates a new RSA key pair
   * 
   * Creates a new public/private key pair for asymmetric encryption
   * and key exchange purposes.
   */
  void
  GenerateRSAKeys();

  /**
   * @brief Returns the public key as a PEM-encoded string
   * @return String containing the PEM-encoded public key
   */
  std::string
  GetPublicKeyString() const;

  /**
   * @brief Loads a peer's public key from a PEM-encoded string
   * @param pemKey The PEM-encoded public key of the peer
   */
  void
  LoadPeerPublicKey(const std::string& pemKey);

  // AES
  /**
   * @brief Generates a new random AES key
   * 
   * Creates a cryptographically secure random key for
   * AES-256 symmetric encryption.
   */
  void
  GenerateAESKey();

  /**
   * @brief Encrypts the AES key using the peer's public RSA key
   * @return Vector of bytes containing the encrypted AES key
   */
  std::vector<unsigned char>
  EncryptAESKeyWithPeer();

  /**
   * @brief Decrypts an AES key using own private RSA key
   * @param encryptedKey The encrypted AES key received from peer
   */
  void
  DecryptAESKey(const std::vector<unsigned char>& encryptedKey);

  /**
   * @brief Encrypts a plaintext message using AES
   * @param plaintext The message to encrypt
   * @param outIV Output parameter that will contain the initialization vector
   * @return Vector of bytes containing the encrypted message
   */
  std::vector<unsigned char>
  AESEncrypt(const std::string& plaintext, std::vector<unsigned char>& outIV);

  /**
   * @brief Decrypts an AES-encrypted message
   * @param ciphertext The encrypted message as bytes
   * @param iv The initialization vector used for encryption
   * @return The decrypted plaintext message
   */
  std::string
  AESDecrypt(const std::vector<unsigned char>& ciphertext,
      const std::vector<unsigned char>& iv);

private:
  RSA* rsaKeyPair;         ///< Own RSA key pair (public and private)
  RSA* peerPublicKey;      ///< Peer's RSA public key
  unsigned char aesKey[32]; ///< AES-256 symmetric key (256 bits = 32 bytes)
};