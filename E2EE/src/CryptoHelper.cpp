#include "CryptoHelper.h"
#include "openssl/pem.h"
#include "openssl/rand.h"
#include "openssl/err.h"
#include "openssl/evp.h"

// Constructor initializes RSA key pointers to nullptr and zeroes the AES key
CryptoHelper::CryptoHelper() :rsaKeyPair(nullptr), 
															peerPublicKey(nullptr) {
	std::memset(&aesKey, 0, sizeof(aesKey));
}

// Destructor frees RSA key structures to prevent memory leaks
CryptoHelper::~CryptoHelper() {
	if (rsaKeyPair) {
		RSA_free(rsaKeyPair);	
	}
	if (peerPublicKey) {
		RSA_free(peerPublicKey);
	}
}

// Generates a new 2048-bit RSA key pair for asymmetric encryption
void
CryptoHelper::GenerateRSAKeys() {
	BIGNUM* bn = BN_new();
	BN_set_word(bn, RSA_F4); // RSA_F4 is the exponent value 65537
	rsaKeyPair = RSA_new();
	RSA_generate_key_ex(rsaKeyPair, 2048, bn, nullptr);
	BN_free(bn);
}

// Exports the public key as a PEM-encoded string for sharing with peers
std::string
CryptoHelper::GetPublicKeyString() const {
	BIO* bio = BIO_new(BIO_s_mem());
	PEM_write_bio_RSAPublicKey(bio, rsaKeyPair);
	char* buffer = nullptr; // KeyData
	size_t length = BIO_get_mem_data(bio, &buffer);
	std::string publicKey(buffer, length);
	BIO_free(bio);
	return publicKey;
}

// Imports a peer's public key from a PEM-encoded string
void
CryptoHelper::LoadPeerPublicKey(const std::string& pemKey) {
	BIO* bio = BIO_new_mem_buf(pemKey.data(), static_cast<int>(pemKey.size()));
	peerPublicKey = PEM_read_bio_RSAPublicKey(bio, nullptr, nullptr, nullptr);
	BIO_free(bio);
	if (!peerPublicKey) {
		throw std::runtime_error("Failed to load peer public key: "
			+ std::string(ERR_error_string(ERR_get_error(), nullptr)));
	}
}

// Generates a cryptographically secure random 256-bit AES key
void
CryptoHelper::GenerateAESKey() {
	RAND_bytes(aesKey, sizeof(aesKey));
}

// Encrypts the AES key with the peer's RSA public key for secure key exchange
std::vector<unsigned char>
CryptoHelper::EncryptAESKeyWithPeer() {
	if (!peerPublicKey) {
		throw std::runtime_error("Peer public key is not loaded.");
	}
	std::vector<unsigned char> encryptedKey(256);
	int result = RSA_public_encrypt(sizeof(aesKey),
																	aesKey,
																	encryptedKey.data(),
																	peerPublicKey,
																	RSA_PKCS1_OAEP_PADDING);
	encryptedKey.resize(result);

	return encryptedKey;
}

// Decrypts an AES key using our private RSA key
void
CryptoHelper::DecryptAESKey(const std::vector<unsigned char>& encryptedKey) {
	RSA_private_decrypt(encryptedKey.size(),
											encryptedKey.data(),
											aesKey,
											rsaKeyPair,
											RSA_PKCS1_OAEP_PADDING);
}

// Encrypts a plaintext message using AES-256-CBC with a random IV
// The IV is returned via the outIV parameter for transmission with the ciphertext
std::vector<unsigned char>
CryptoHelper::AESEncrypt(const std::string& plaintext,
												 std::vector<unsigned char>& outIV) {
	// Generate a random initialization vector (IV)
	outIV.resize(AES_BLOCK_SIZE);
	RAND_bytes(outIV.data(), AES_BLOCK_SIZE);

	const EVP_CIPHER* cipher = EVP_aes_256_cbc();
	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();

	// Allocate enough space for the encrypted data including padding
	std::vector<unsigned char> out(plaintext.size() + AES_BLOCK_SIZE); // +pad
	int outlen1 = 0, outlen2 = 0;

	// Initialize encryption context
	EVP_EncryptInit_ex(ctx, 
										 cipher, 
										 nullptr, 
									   aesKey, 
										 outIV.data());

	// Encrypt the data
	EVP_EncryptUpdate(ctx,
										out.data(), &outlen1,
										reinterpret_cast<const unsigned char*>(plaintext.data()),
										static_cast<int>(plaintext.size()));

	// Finalize encryption and add padding
	EVP_EncryptFinal_ex(ctx, out.data() + outlen1, &outlen2);

	// Resize output to actual encrypted size
	out.resize(outlen1 + outlen2);
	EVP_CIPHER_CTX_free(ctx);
	return out;
}

// Decrypts AES-encrypted data using the provided IV
std::string
CryptoHelper::AESDecrypt(const std::vector<unsigned char>& ciphertext,
												 const std::vector<unsigned char>& iv) {
	const EVP_CIPHER* cipher = EVP_aes_256_cbc();
	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();

	std::vector<unsigned char> out(ciphertext.size());
	int outlen1 = 0, outlen2 = 0;

	// Initialize decryption context
	EVP_DecryptInit_ex(ctx, 
										 cipher, 
										 nullptr, 
										 aesKey, 
										 iv.data());

	// Decrypt the data
	EVP_DecryptUpdate(ctx,
									  out.data(), 
										&outlen1,
										ciphertext.data(),
										static_cast<int>(ciphertext.size()));
	
	// Finalize decryption and verify padding
	if (EVP_DecryptFinal_ex(ctx, out.data() + outlen1, &outlen2) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		return {}; // Return empty string if decryption fails (incorrect padding/key/iv)
	}

	// Resize output to actual decrypted size and convert to string
	out.resize(outlen1 + outlen2);
	EVP_CIPHER_CTX_free(ctx);
	return std::string(reinterpret_cast<char*>(out.data()), out.size());
}
