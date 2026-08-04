#include "CryptoHelper.h"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

#include <algorithm>
#include <iomanip>
#include <memory>
#include <sstream>

namespace {
constexpr unsigned char kMessageAad[] = "E2EE-MSG-v2";

std::string OpenSSLError(const char* operation) {
  const unsigned long code = ERR_get_error();
  char detail[256]{};
  if (code != 0) ERR_error_string_n(code, detail, sizeof(detail));
  return std::string(operation) + (code ? ": " + std::string(detail) : " failed");
}
}

CryptoHelper::CryptoHelper() = default;

CryptoHelper::~CryptoHelper() {
  EVP_PKEY_free(m_keyPair);
  EVP_PKEY_free(m_peerPublicKey);
  OPENSSL_cleanse(m_aesKey, sizeof(m_aesKey));
}

void CryptoHelper::Require(bool result, const char* operation) {
  if (!result) throw std::runtime_error(OpenSSLError(operation));
}

void CryptoHelper::GenerateRSAKeys() {
  EVP_PKEY_CTX* rawContext = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
  Require(rawContext != nullptr, "EVP_PKEY_CTX_new_id");
  std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>
    context(rawContext, EVP_PKEY_CTX_free);
  Require(EVP_PKEY_keygen_init(context.get()) == 1, "EVP_PKEY_keygen_init");
  Require(EVP_PKEY_CTX_set_rsa_keygen_bits(context.get(), 3072) == 1,
          "EVP_PKEY_CTX_set_rsa_keygen_bits");
  EVP_PKEY* generated = nullptr;
  Require(EVP_PKEY_keygen(context.get(), &generated) == 1, "EVP_PKEY_keygen");
  EVP_PKEY_free(m_keyPair);
  m_keyPair = generated;
}

std::string CryptoHelper::GetPublicKeyString() const {
  Require(m_keyPair != nullptr, "public key not generated");
  BIO* rawBio = BIO_new(BIO_s_mem());
  Require(rawBio != nullptr, "BIO_new");
  std::unique_ptr<BIO, decltype(&BIO_free)> bio(rawBio, BIO_free);
  Require(PEM_write_bio_PUBKEY(bio.get(), m_keyPair) == 1,
          "PEM_write_bio_PUBKEY");
  char* buffer = nullptr;
  const long length = BIO_get_mem_data(bio.get(), &buffer);
  Require(length > 0 && buffer != nullptr, "BIO_get_mem_data");
  return std::string(buffer, static_cast<size_t>(length));
}

void CryptoHelper::LoadPeerPublicKey(const std::string& pemKey) {
  BIO* rawBio = BIO_new_mem_buf(pemKey.data(), static_cast<int>(pemKey.size()));
  Require(rawBio != nullptr, "BIO_new_mem_buf");
  std::unique_ptr<BIO, decltype(&BIO_free)> bio(rawBio, BIO_free);
  EVP_PKEY* loaded = PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr);
  Require(loaded != nullptr, "PEM_read_bio_PUBKEY");
  if (EVP_PKEY_base_id(loaded) != EVP_PKEY_RSA || EVP_PKEY_bits(loaded) < 3072) {
    EVP_PKEY_free(loaded);
    throw std::runtime_error("Peer key must be RSA with at least 3072 bits");
  }
  EVP_PKEY_free(m_peerPublicKey);
  m_peerPublicKey = loaded;
}

void CryptoHelper::GenerateAESKey() {
  Require(RAND_priv_bytes(m_aesKey, sizeof(m_aesKey)) == 1, "RAND_priv_bytes");
  m_hasAESKey = true;
}

std::vector<unsigned char> CryptoHelper::EncryptAESKeyWithPeer() const {
  Require(m_peerPublicKey != nullptr, "peer public key not loaded");
  Require(m_hasAESKey, "AES key not generated");
  EVP_PKEY_CTX* rawContext = EVP_PKEY_CTX_new(m_peerPublicKey, nullptr);
  Require(rawContext != nullptr, "EVP_PKEY_CTX_new");
  std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>
    context(rawContext, EVP_PKEY_CTX_free);
  Require(EVP_PKEY_encrypt_init(context.get()) == 1, "EVP_PKEY_encrypt_init");
  Require(EVP_PKEY_CTX_set_rsa_padding(context.get(), RSA_PKCS1_OAEP_PADDING) == 1,
          "set RSA OAEP padding");
  Require(EVP_PKEY_CTX_set_rsa_oaep_md(context.get(), EVP_sha256()) == 1,
          "set RSA OAEP digest");
  Require(EVP_PKEY_CTX_set_rsa_mgf1_md(context.get(), EVP_sha256()) == 1,
          "set RSA MGF1 digest");
  size_t outputSize = 0;
  Require(EVP_PKEY_encrypt(context.get(), nullptr, &outputSize,
                           m_aesKey, sizeof(m_aesKey)) == 1,
          "measure encrypted session key");
  std::vector<unsigned char> encrypted(outputSize);
  Require(EVP_PKEY_encrypt(context.get(), encrypted.data(), &outputSize,
                           m_aesKey, sizeof(m_aesKey)) == 1,
          "encrypt session key");
  encrypted.resize(outputSize);
  return encrypted;
}

void CryptoHelper::DecryptAESKey(const std::vector<unsigned char>& encryptedKey) {
  Require(m_keyPair != nullptr, "private key not generated");
  EVP_PKEY_CTX* rawContext = EVP_PKEY_CTX_new(m_keyPair, nullptr);
  Require(rawContext != nullptr, "EVP_PKEY_CTX_new");
  std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>
    context(rawContext, EVP_PKEY_CTX_free);
  Require(EVP_PKEY_decrypt_init(context.get()) == 1, "EVP_PKEY_decrypt_init");
  Require(EVP_PKEY_CTX_set_rsa_padding(context.get(), RSA_PKCS1_OAEP_PADDING) == 1,
          "set RSA OAEP padding");
  Require(EVP_PKEY_CTX_set_rsa_oaep_md(context.get(), EVP_sha256()) == 1,
          "set RSA OAEP digest");
  Require(EVP_PKEY_CTX_set_rsa_mgf1_md(context.get(), EVP_sha256()) == 1,
          "set RSA MGF1 digest");
  size_t outputSize = 0;
  Require(EVP_PKEY_decrypt(context.get(), nullptr, &outputSize,
                           encryptedKey.data(), encryptedKey.size()) == 1,
          "measure decrypted session key");
  std::vector<unsigned char> decrypted(outputSize);
  Require(EVP_PKEY_decrypt(context.get(), decrypted.data(), &outputSize,
                           encryptedKey.data(), encryptedKey.size()) == 1,
          "decrypt session key");
  Require(outputSize == sizeof(m_aesKey), "invalid AES session key length");
  std::copy_n(decrypted.data(), sizeof(m_aesKey), m_aesKey);
  OPENSSL_cleanse(decrypted.data(), decrypted.size());
  m_hasAESKey = true;
}

std::vector<unsigned char>
CryptoHelper::EncryptMessage(const std::string& plaintext) const {
  Require(m_hasAESKey, "AES key not available");
  Require(plaintext.size() <= INT_MAX, "message too large");
  std::vector<unsigned char> nonce(GcmNonceSize);
  Require(RAND_bytes(nonce.data(), static_cast<int>(nonce.size())) == 1,
          "RAND_bytes");
  EVP_CIPHER_CTX* rawContext = EVP_CIPHER_CTX_new();
  Require(rawContext != nullptr, "EVP_CIPHER_CTX_new");
  std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>
    context(rawContext, EVP_CIPHER_CTX_free);
  Require(EVP_EncryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1,
          "EVP_EncryptInit_ex");
  Require(EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_IVLEN,
                              static_cast<int>(nonce.size()), nullptr) == 1,
          "set GCM nonce length");
  Require(EVP_EncryptInit_ex(context.get(), nullptr, nullptr, m_aesKey, nonce.data()) == 1,
          "initialize AES-256-GCM");
  int ignored = 0;
  Require(EVP_EncryptUpdate(context.get(), nullptr, &ignored, kMessageAad,
                            static_cast<int>(sizeof(kMessageAad) - 1)) == 1,
          "authenticate protocol header");
  std::vector<unsigned char> ciphertext(plaintext.size());
  int written = 0;
  Require(EVP_EncryptUpdate(context.get(), ciphertext.data(), &written,
    reinterpret_cast<const unsigned char*>(plaintext.data()),
    static_cast<int>(plaintext.size())) == 1, "encrypt message");
  int finalWritten = 0;
  Require(EVP_EncryptFinal_ex(context.get(), ciphertext.data() + written,
                              &finalWritten) == 1, "finalize GCM encryption");
  ciphertext.resize(static_cast<size_t>(written + finalWritten));
  std::vector<unsigned char> tag(GcmTagSize);
  Require(EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_GET_TAG,
                              static_cast<int>(tag.size()), tag.data()) == 1,
          "read GCM authentication tag");

  std::vector<unsigned char> packet;
  packet.reserve(1 + nonce.size() + tag.size() + ciphertext.size());
  packet.push_back(MessageVersion);
  packet.insert(packet.end(), nonce.begin(), nonce.end());
  packet.insert(packet.end(), tag.begin(), tag.end());
  packet.insert(packet.end(), ciphertext.begin(), ciphertext.end());
  return packet;
}

bool CryptoHelper::DecryptMessage(const std::vector<unsigned char>& packet,
                                  std::string& plaintext) const {
  plaintext.clear();
  if (!m_hasAESKey || packet.size() < 1 + GcmNonceSize + GcmTagSize ||
      packet[0] != MessageVersion) return false;
  const unsigned char* nonce = packet.data() + 1;
  const unsigned char* tag = nonce + GcmNonceSize;
  const unsigned char* ciphertext = tag + GcmTagSize;
  const size_t ciphertextSize = packet.size() - 1 - GcmNonceSize - GcmTagSize;
  if (ciphertextSize > INT_MAX) return false;

  EVP_CIPHER_CTX* rawContext = EVP_CIPHER_CTX_new();
  if (!rawContext) return false;
  std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>
    context(rawContext, EVP_CIPHER_CTX_free);
  if (EVP_DecryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
      EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_IVLEN,
                          static_cast<int>(GcmNonceSize), nullptr) != 1 ||
      EVP_DecryptInit_ex(context.get(), nullptr, nullptr, m_aesKey, nonce) != 1) return false;
  int ignored = 0;
  if (EVP_DecryptUpdate(context.get(), nullptr, &ignored, kMessageAad,
                        static_cast<int>(sizeof(kMessageAad) - 1)) != 1) return false;
  std::vector<unsigned char> output(ciphertextSize + 1);
  int written = 0;
  if (EVP_DecryptUpdate(context.get(), output.data(), &written, ciphertext,
                        static_cast<int>(ciphertextSize)) != 1) return false;
  std::vector<unsigned char> mutableTag(tag, tag + GcmTagSize);
  if (EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_TAG,
                          static_cast<int>(mutableTag.size()), mutableTag.data()) != 1) return false;
  int finalWritten = 0;
  if (EVP_DecryptFinal_ex(context.get(), output.data() + written, &finalWritten) != 1) {
    return false;
  }
  output.resize(static_cast<size_t>(written + finalWritten));
  plaintext.assign(reinterpret_cast<const char*>(output.data()), output.size());
  return true;
}

std::vector<unsigned char> CryptoHelper::PublicKeyDer(EVP_PKEY* key) {
  Require(key != nullptr, "public key not available");
  const int length = i2d_PUBKEY(key, nullptr);
  Require(length > 0, "i2d_PUBKEY");
  std::vector<unsigned char> der(static_cast<size_t>(length));
  unsigned char* output = der.data();
  Require(i2d_PUBKEY(key, &output) == length, "i2d_PUBKEY");
  return der;
}

std::string CryptoHelper::Fingerprint(const std::vector<unsigned char>& bytes,
                                      size_t groups) {
  unsigned char digest[SHA256_DIGEST_LENGTH]{};
  SHA256(bytes.data(), bytes.size(), digest);
  std::ostringstream result;
  result << std::uppercase << std::hex << std::setfill('0');
  groups = std::min(groups, static_cast<size_t>(SHA256_DIGEST_LENGTH));
  for (size_t index = 0; index < groups; ++index) {
    if (index) result << '-';
    result << std::setw(2) << static_cast<unsigned int>(digest[index]);
  }
  return result.str();
}

std::string CryptoHelper::GetOwnKeyFingerprint() const {
  return Fingerprint(PublicKeyDer(m_keyPair));
}

std::string CryptoHelper::GetPeerKeyFingerprint() const {
  return Fingerprint(PublicKeyDer(m_peerPublicKey));
}

std::string CryptoHelper::GetSessionSafetyNumber() const {
  Require(m_hasAESKey && m_keyPair && m_peerPublicKey, "session is not ready");
  auto own = PublicKeyDer(m_keyPair);
  auto peer = PublicKeyDer(m_peerPublicKey);
  if (peer < own) std::swap(own, peer);
  std::vector<unsigned char> transcript;
  transcript.reserve(own.size() + peer.size() + sizeof(m_aesKey));
  transcript.insert(transcript.end(), own.begin(), own.end());
  transcript.insert(transcript.end(), peer.begin(), peer.end());
  transcript.insert(transcript.end(), m_aesKey, m_aesKey + sizeof(m_aesKey));
  return Fingerprint(transcript, 12);
}
