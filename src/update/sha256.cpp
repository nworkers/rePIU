#include "repiu/update/sha256.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

namespace repiu::update
{
namespace
{

constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU,
    0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U,
    0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U,
    0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U,
    0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U,
    0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U,
    0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU,
    0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

constexpr std::uint32_t RotateRight(std::uint32_t value, int bits)
{
    return (value >> bits) | (value << (32 - bits));
}

}  // namespace

Sha256::Sha256()
    : state_{0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
             0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U}
{
}

void Sha256::Compress(const std::uint8_t* block)
{
    std::array<std::uint32_t, 64> schedule{};
    for (std::size_t index = 0; index < 16U; ++index)
    {
        schedule[index] = (static_cast<std::uint32_t>(block[index * 4U]) << 24) |
            (static_cast<std::uint32_t>(block[index * 4U + 1U]) << 16) |
            (static_cast<std::uint32_t>(block[index * 4U + 2U]) << 8) |
            static_cast<std::uint32_t>(block[index * 4U + 3U]);
    }
    for (std::size_t index = 16; index < 64U; ++index)
    {
        const std::uint32_t s0 = RotateRight(schedule[index - 15U], 7) ^
            RotateRight(schedule[index - 15U], 18) ^
            (schedule[index - 15U] >> 3);
        const std::uint32_t s1 = RotateRight(schedule[index - 2U], 17) ^
            RotateRight(schedule[index - 2U], 19) ^ (schedule[index - 2U] >> 10);
        schedule[index] =
            schedule[index - 16U] + s0 + schedule[index - 7U] + s1;
    }
    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];
    std::uint32_t f = state_[5];
    std::uint32_t g = state_[6];
    std::uint32_t h = state_[7];
    for (std::size_t index = 0; index < 64U; ++index)
    {
        const std::uint32_t s1 =
            RotateRight(e, 6) ^ RotateRight(e, 11) ^ RotateRight(e, 25);
        const std::uint32_t choice = (e & f) ^ (~e & g);
        const std::uint32_t temp1 =
            h + s1 + choice + kRoundConstants[index] + schedule[index];
        const std::uint32_t s0 =
            RotateRight(a, 2) ^ RotateRight(a, 13) ^ RotateRight(a, 22);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = s0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256::Update(const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    total_bytes_ += size;
    while (size > 0U)
    {
        const std::size_t take = std::min(size, buffer_.size() - buffered_);
        std::memcpy(buffer_.data() + buffered_, bytes, take);
        buffered_ += take;
        bytes += take;
        size -= take;
        if (buffered_ == buffer_.size())
        {
            Compress(buffer_.data());
            buffered_ = 0;
        }
    }
}

std::array<std::uint8_t, 32> Sha256::Finish()
{
    const std::uint64_t bit_length = total_bytes_ * 8U;
    const std::uint8_t terminator = 0x80U;
    Update(&terminator, 1);
    const std::uint8_t zero = 0;
    while (buffered_ != 56U)
    {
        Update(&zero, 1);
    }
    std::array<std::uint8_t, 8> length{};
    for (std::size_t index = 0; index < 8U; ++index)
    {
        length[index] =
            static_cast<std::uint8_t>(bit_length >> (56U - index * 8U));
    }
    Update(length.data(), length.size());
    std::array<std::uint8_t, 32> digest{};
    for (std::size_t index = 0; index < 8U; ++index)
    {
        digest[index * 4U] = static_cast<std::uint8_t>(state_[index] >> 24);
        digest[index * 4U + 1U] = static_cast<std::uint8_t>(state_[index] >> 16);
        digest[index * 4U + 2U] = static_cast<std::uint8_t>(state_[index] >> 8);
        digest[index * 4U + 3U] = static_cast<std::uint8_t>(state_[index]);
    }
    return digest;
}

std::string FormatSha256(const std::array<std::uint8_t, 32>& digest)
{
    constexpr char kHex[] = "0123456789abcdef";
    std::string text;
    text.reserve(64);
    for (std::uint8_t byte : digest)
    {
        text.push_back(kHex[byte >> 4]);
        text.push_back(kHex[byte & 0x0FU]);
    }
    return text;
}

std::optional<std::string> Sha256OfFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return std::nullopt;
    }
    Sha256 hash;
    std::vector<char> block(1U << 16);
    while (stream)
    {
        stream.read(block.data(), static_cast<std::streamsize>(block.size()));
        const std::streamsize got = stream.gcount();
        if (got > 0)
        {
            hash.Update(block.data(), static_cast<std::size_t>(got));
        }
    }
    if (stream.bad())
    {
        return std::nullopt;
    }
    return FormatSha256(hash.Finish());
}

}  // namespace repiu::update
