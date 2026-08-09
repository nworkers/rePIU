#ifndef REPIU_SOUND_STREAM_CHUNK_AUDIT_H_
#define REPIU_SOUND_STREAM_CHUNK_AUDIT_H_

#include <cstddef>
#include <cstdint>
#include <span>

namespace repiu::sound
{

struct StreamChunkDigest
{
    std::uint64_t chunk_index = 0U;
    std::uint64_t end_offset = 0U;
    std::uint64_t hash = 0U;
    std::size_t byte_count = 0U;
};

class StreamChunkAudit
{
public:
    static constexpr std::size_t kChunkBytes = 4096U;
    using DigestSink = void (*)(const void*, const StreamChunkDigest&);

    void Reset();
    void Consume(std::span<const std::uint8_t> bytes,
                 const void* sink_context,
                 DigestSink sink);
    StreamChunkDigest partial_digest() const;
    std::uint64_t total_bytes() const { return total_bytes_; }

private:
    static constexpr std::uint64_t kFnvOffsetBasis =
        14695981039346656037ULL;
    static constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

    std::uint64_t chunk_index_ = 0U;
    std::uint64_t total_bytes_ = 0U;
    std::uint64_t hash_ = kFnvOffsetBasis;
    std::size_t chunk_bytes_ = 0U;
};

}  // namespace repiu::sound

#endif  // REPIU_SOUND_STREAM_CHUNK_AUDIT_H_
