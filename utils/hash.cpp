#include "utils/hash.h"
#include "utils/file_utils.h"

#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <array>
#include <vector>

namespace yag::utils {

// ============================================================
// Self-contained SHA-256 implementation
// Reference: FIPS PUB 180-4
// ============================================================

namespace {

// SHA-256 round constants (first 32 bits of the fractional parts
// of the cube roots of the first 64 primes)
constexpr std::array<uint32_t, 64> K = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
inline uint32_t sigma0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
inline uint32_t sigma1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
inline uint32_t gamma0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
inline uint32_t gamma1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

// Compute SHA-256 of raw bytes, returns 32-byte digest
std::array<uint8_t, 32> sha256_bytes(const uint8_t* data, size_t len) {
    // Initial hash values (first 32 bits of the fractional parts
    // of the square roots of the first 8 primes)
    uint32_t h0 = 0x6a09e667, h1 = 0xbb67ae85,
             h2 = 0x3c6ef372, h3 = 0xa54ff53a,
             h4 = 0x510e527f, h5 = 0x9b05688c,
             h6 = 0x1f83d9ab, h7 = 0x5be0cd19;

    // Pre-processing: padding
    // message + 1 byte (0x80) + padding zeros + 8 bytes (length in bits)
    // total must be multiple of 64 bytes (512 bits)
    uint64_t bit_len = static_cast<uint64_t>(len) * 8;
    size_t padded_len = len + 1;  // +1 for the 0x80 byte
    while (padded_len % 64 != 56) {
        padded_len++;
    }
    padded_len += 8;  // 8 bytes for the length

    std::vector<uint8_t> msg(padded_len, 0);
    std::memcpy(msg.data(), data, len);
    msg[len] = 0x80;

    // Append length in bits as big-endian 64-bit
    for (int i = 0; i < 8; i++) {
        msg[padded_len - 1 - i] = static_cast<uint8_t>(bit_len >> (i * 8));
    }

    // Process each 512-bit (64-byte) block
    for (size_t offset = 0; offset < padded_len; offset += 64) {
        // Prepare message schedule (64 words)
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = (static_cast<uint32_t>(msg[offset + i * 4    ]) << 24)
                 | (static_cast<uint32_t>(msg[offset + i * 4 + 1]) << 16)
                 | (static_cast<uint32_t>(msg[offset + i * 4 + 2]) << 8)
                 | (static_cast<uint32_t>(msg[offset + i * 4 + 3]));
        }
        for (int i = 16; i < 64; i++) {
            w[i] = gamma1(w[i-2]) + w[i-7] + gamma0(w[i-15]) + w[i-16];
        }

        // Working variables
        uint32_t a = h0, b = h1, c = h2, d = h3,
                 e = h4, f = h5, g = h6, h = h7;

        // 64 rounds of compression
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = h + sigma1(e) + ch(e, f, g) + K[i] + w[i];
            uint32_t t2 = sigma0(a) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        // Add compressed chunk to hash
        h0 += a; h1 += b; h2 += c; h3 += d;
        h4 += e; h5 += f; h6 += g; h7 += h;
    }

    // Produce the 32-byte digest (big-endian)
    std::array<uint8_t, 32> digest;
    auto store = [&](int pos, uint32_t val) {
        digest[pos    ] = static_cast<uint8_t>(val >> 24);
        digest[pos + 1] = static_cast<uint8_t>(val >> 16);
        digest[pos + 2] = static_cast<uint8_t>(val >> 8);
        digest[pos + 3] = static_cast<uint8_t>(val);
    };
    store(0, h0);   store(4, h1);   store(8, h2);   store(12, h3);
    store(16, h4);  store(20, h5);  store(24, h6);  store(28, h7);

    return digest;
}

// Convert 32-byte digest to 64-char hex string
std::string digest_to_hex(const std::array<uint8_t, 32>& digest) {
    std::ostringstream oss;
    for (uint8_t byte : digest) {
        oss << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<int>(byte);
    }
    return oss.str();
}

} // anonymous namespace

// ============================================================
// Public API
// ============================================================

std::string hash_string(const std::string& content) {
    auto digest = sha256_bytes(
        reinterpret_cast<const uint8_t*>(content.data()),
        content.size()
    );
    return digest_to_hex(digest);
}

std::string hash_file(const std::filesystem::path& filepath) {
    std::string content = read_file(filepath);
    return hash_string(content);
}

} // namespace yag::utils
