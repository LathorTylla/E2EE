#include "openssl/crypto.h"
#include <iostream>

int
main() {
  std::cout << "OpenSSL version: " << OpenSSL_version(OPENSSL_VERSION) << std::endl;
  return 0;
}