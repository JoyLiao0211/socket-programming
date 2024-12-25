#ifndef SSL_HPP
#define SSL_HPP

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <iostream>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <string>

#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>

void initialize_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl() {
    EVP_cleanup();
}
SSL_CTX* create_ssl_server_context() {
    const SSL_METHOD* method = TLS_server_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

SSL_CTX* create_ssl_client_context() {
    const SSL_METHOD* method = TLS_client_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

void configure_ssl_context(SSL_CTX* ctx, std::string hostname) {
    std::string cert = "certs/" + hostname + ".pem", key = "certs/" + hostname + ".key"; 
    if (SSL_CTX_use_certificate_file(ctx, cert.c_str(), SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, key.c_str(), SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

void generate_cert(std::string hostname) {
    EVP_PKEY *pkey = EVP_RSA_gen(2048);

    X509 *x509 = X509_new();
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 31536000L); // Valid for 1 year
    X509_set_pubkey(x509, pkey);

    X509_NAME *name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char *)"TW", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char *)"Computer Networking", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *)"localhost", -1, -1, 0);
    X509_set_issuer_name(x509, name);

    X509_sign(x509, pkey, EVP_sha256());

    std::string cert = "certs/" + hostname + ".pem", key = "certs/" + hostname + ".key";

    FILE *pkey_file = fopen(key.c_str(), "wb");
    PEM_write_PrivateKey(pkey_file, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    fclose(pkey_file);

    FILE *cert_file = fopen(cert.c_str(), "wb");
    PEM_write_X509(cert_file, x509);
    fclose(cert_file);

    EVP_PKEY_free(pkey);
    X509_free(x509);
}

#endif
