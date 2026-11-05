// The services of the outside world for the smoke test of the Docker image with accounts
// (docker/smoke-test-accounts.sh): a fake Discord over HTTPS on port 443 and a fake mail server in the clear
// on port 25, on every address of its container. The test gives this container the network alias discord.com,
// and the image of the server trusts the certificate written here: the master reaches it at the address of
// Discord it always uses, which neither its settings nor its command line can change.
//
//   rtype_fake_services <folder>
//
// It writes the certificate it serves, made at start for discord.com, to <folder>/certificate.pem, then
// "ready" on its standard output, and a line for each mail it receives: "mail <recipient>". It never runs
// more than ten minutes: a test that fails leaves no program behind for long.

#include "FakeDiscord.hpp"
#include "SmtpServer.hpp"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{

constexpr int Usage = 2;
constexpr int Failed = 1;
constexpr std::chrono::minutes Lifetime{10};

/// The names the certificate is good for: those of Discord the master asks.
constexpr const char *Names = "DNS:discord.com,DNS:localhost,IP:127.0.0.1";

void writePem(const std::filesystem::path &path, auto write)
{
    FILE *file = std::fopen(path.string().c_str(), "wb");
    if (file == nullptr || write(file) != 1) {
        if (file != nullptr) {
            std::fclose(file);
        }
        throw std::runtime_error("Cannot write " + path.string());
    }
    std::fclose(file);
}

/// A key and a certificate signed by itself, for Names, a day long: the image of the server adds it to the
/// authorities it trusts. Written as PEM, the certificate readable by the test.
void makeCertificate(const std::filesystem::path &certificateFile, const std::filesystem::path &keyFile)
{
    EVP_PKEY *key = EVP_EC_gen("P-256");
    X509 *certificate = X509_new();
    X509_set_version(certificate, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(certificate), 1);
    X509_gmtime_adj(X509_getm_notBefore(certificate), -60);
    X509_gmtime_adj(X509_getm_notAfter(certificate), 24L * 60 * 60);
    X509_set_pubkey(certificate, key);
    X509_NAME *name = X509_get_subject_name(certificate);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char *>("discord.com"), -1, -1, 0);
    X509_set_issuer_name(certificate, name);
    X509V3_CTX context;
    X509V3_set_ctx_nodb(&context);
    X509V3_set_ctx(&context, certificate, certificate, nullptr, nullptr, 0);
    for (const auto &[nid, value] :
         {std::pair{NID_subject_alt_name, Names}, std::pair{NID_basic_constraints, "critical,CA:TRUE"}}) {
        X509_EXTENSION *extension = X509V3_EXT_conf_nid(nullptr, &context, nid, value);
        X509_add_ext(certificate, extension, -1);
        X509_EXTENSION_free(extension);
    }
    X509_sign(certificate, key, EVP_sha256());
    writePem(certificateFile, [&](FILE *file) { return PEM_write_X509(file, certificate); });
    writePem(keyFile, [&](FILE *file) {
        return PEM_write_PrivateKey(file, key, nullptr, nullptr, 0, nullptr, nullptr);
    });
    X509_free(certificate);
    EVP_PKEY_free(key);
}

int run(const std::filesystem::path &folder)
{
    const std::filesystem::path certificateFile = folder / "certificate.pem";
    const std::filesystem::path keyFile = folder / "key.pem";
    makeCertificate(certificateFile, keyFile);

    const testing::FakeDiscord discord(
        testing::FakeDiscordEndpoint{.address = "0.0.0.0",
                                     .port = 443,
                                     .certificateFile = certificateFile.string(),
                                     .keyFile = keyFile.string()});
    testing::SmtpServer mail(testing::SmtpSettings{
        .tls = testing::SmtpTls::None, .username = {}, .password = {}, .address = "0.0.0.0", .port = 25});
    std::cout << "ready" << std::endl;

    std::size_t told = 0;
    const auto deadline = std::chrono::steady_clock::now() + Lifetime;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto mails = mail.waitForMails(told + 1, std::chrono::seconds(1));
        for (; told < mails.size(); ++told) {
            for (const std::string &recipient : mails[told].to) {
                std::cout << "mail " << recipient << std::endl;
            }
        }
    }
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "Usage: rtype_fake_services <folder>\n";
        return Usage;
    }
    try {
        return run(argv[1]);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return Failed;
    }
}
