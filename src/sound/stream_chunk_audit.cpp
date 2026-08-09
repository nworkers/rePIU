#include "repiu/sound/stream_chunk_audit.h"

namespace repiu::sound
{

void StreamChunkAudit::Reset()
{
    chunk_index_ = 0U;
    total_bytes_ = 0U;
    hash_ = kFnvOffsetBasis;
    chunk_bytes_ = 0U;
}

void StreamChunkAudit::Consume(std::span<const std::uint8_t> bytes,
                               const void* sink_context, DigestSink sink)
{
    for (const std::uint8_t byte : bytes)
    {
        hash_ ^= byte;
        hash_ *= kFnvPrime;
        ++chunk_bytes_;
        ++total_bytes_;
        if (chunk_bytes_ == kChunkBytes)
        {
            if (sink != nullptr)
            {
                sink(sink_context, StreamChunkDigest{
                    chunk_index_, total_bytes_, hash_, chunk_bytes_});
            }
            ++chunk_index_;
            hash_ = kFnvOffsetBasis;
            chunk_bytes_ = 0U;
        }
    }
}

StreamChunkDigest StreamChunkAudit::partial_digest() const
{
    return StreamChunkDigest{
        chunk_index_, total_bytes_, hash_, chunk_bytes_};
}

}  // namespace repiu::sound
