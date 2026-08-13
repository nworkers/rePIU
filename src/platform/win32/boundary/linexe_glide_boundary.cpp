#include "linexe_glide_boundary.h"
#include "aot/aot_runtime_dispatch.h"
#include "execution_internal.h"
#include "guest_memory_access.h"
#include "instruction_emulation.h"

#include "repiu/hle/glide_texture_decode.h"
#include "repiu/hle/glide_implementation_issue.h"
#include "repiu/hle/glide_lfb.h"
#include "repiu/hle/glide_lfb_region.h"
#include "repiu/hle/glide_vertex.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace repiu::platform::win32
{

bool DecodeGlideTexDownloadTableCall(const std::uint32_t* guest_stack,
                                     const std::size_t word_count,
                                     GlideTexDownloadTableCall* call)
{
    constexpr std::size_t kFrameWordCount = 4U;
    if (guest_stack == nullptr || word_count < kFrameWordCount || call == nullptr)
    {
        return false;
    }
    call->tmu = guest_stack[1];
    call->type = guest_stack[2];
    call->data = guest_stack[3];
    call->stack_advance =
        static_cast<std::uint32_t>(kFrameWordCount * sizeof(std::uint32_t));
    return true;
}

namespace
{

// Task 335. Off by default: the host poll loop is the only pump caller now.
// Resolved once because the gate is a hot path.
bool GlideGatePumpEventsEnabled()
{
    static const bool enabled = []() {
        const char* value = std::getenv("REPIU_GLIDE_GATE_PUMP");
        return value != nullptr && std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

class GlideOrdinalTimingScope
{
  public:
    GlideOrdinalTimingScope(Win32GlideOrdinalTimingProfile* profile,
                            GlideOpenGlBackend* backend,
                            const std::uint64_t* gate_cycles)
        : profile_(profile),
          backend_(backend),
          gate_cycles_(gate_cycles)
    {
    }

    ~GlideOrdinalTimingScope()
    {
        if (!active_)
        {
            return;
        }
        RecordGlideOrdinalGateTime(
            profile_, ordinal_, *gate_cycles_);
        backend_->EndGlideOrdinalTiming();
    }

    void Begin(std::uint16_t ordinal)
    {
        if (profile_ == nullptr)
        {
            return;
        }
        ordinal_ = ordinal;
        backend_->BeginGlideOrdinalTiming(profile_, ordinal_);
        active_ = true;
    }

    GlideOrdinalTimingScope(const GlideOrdinalTimingScope&) = delete;
    GlideOrdinalTimingScope& operator=(const GlideOrdinalTimingScope&) = delete;

  private:
    Win32GlideOrdinalTimingProfile* profile_ = nullptr;
    std::uint16_t ordinal_ = 0;
    GlideOpenGlBackend* backend_ = nullptr;
    const std::uint64_t* gate_cycles_ = nullptr;
    bool active_ = false;
};

// Tasks 364 and 365. Hooked once here rather than in each setter case, so no
// dispatch path is edited. It serves two consumers that must agree exactly: the
// Task 364 census, which only observes, and the Task 365 cache, which skips the
// host rendezvous for a confirmed repeat. Building the key once and classifying
// the outcome once is what makes them unable to diverge — and what makes the
// cross-check "census `same` == cache `elided`" a real proof that only observed
// duplicates were skipped.
//
// The outcome is read from the handled counter and the implementation-issue
// totals across the call, so the census still cannot change a result.
class GlideSetterStateScope
{
  public:
    GlideSetterStateScope(Win32GlideSetterCensusProfile* census,
                          Win32GlideSetterStateCache* cache,
                          const ThreadContext* context)
        : census_(census),
          cache_(cache),
          context_(context)
    {
    }

    ~GlideSetterStateScope()
    {
        if (!active_)
        {
            return;
        }
        if (elided_)
        {
            // An elided call is still a call, and still an exact repeat, so the
            // observer counts it as `same` exactly as it would have without the
            // elision. That keeps the census comparable between the two
            // configurations, which gate E1 depends on.
            RecordGlideSetterCensusCall(
                census_, ordinal_, key_,
                Win32GlideSetterCensusOutcome::kApplied);
            return;
        }
        const Win32GlideSetterCensusOutcome outcome = Classify();
        RecordGlideSetterCensusCall(census_, ordinal_, key_, outcome);
        if (!elision_candidate_)
        {
            return;
        }
        if (outcome == Win32GlideSetterCensusOutcome::kApplied)
        {
            RecordGlideSetterStateApplied(cache_, ordinal_, key_);
        }
        else
        {
            // A decline or a retained-but-unexpressed argument leaves the host
            // state unknown, so the record must go rather than be trusted.
            RecordGlideSetterStateVoided(cache_, ordinal_);
        }
    }

    // Called after the gate stack mirror is populated and before dispatch.
    void Begin(const repiu::hle::GlideExportGate& glide_export)
    {
        if (census_ == nullptr && cache_ == nullptr)
        {
            return;
        }
        const repiu::hle::GlideGateId gate_id = glide_export.gate_id;
        // Boundary bookkeeping runs for every gate, not only setter targets.
        if (IsGlideSetterStateInvalidatingGate(gate_id))
        {
            RecordGlideSetterCensusInvalidation(census_);
            InvalidateGlideSetterStateCache(cache_);
        }
        if (IsGlideSetterStateTextureGenerationGate(gate_id))
        {
            RecordGlideSetterCensusTextureGeneration(census_);
            BumpGlideSetterStateCacheTextureGeneration(cache_);
        }
        if (gate_id == repiu::hle::GlideGateId::kGrBufferSwap)
        {
            RecordGlideSetterCensusFrameBoundary(census_);
        }
        if (!IsGlideSetterStateGate(gate_id))
        {
            return;
        }

        ordinal_ = glide_export.ordinal;
        const std::uint32_t argument_words =
            glide_export.argument_byte_count / sizeof(std::uint32_t);
        if (argument_words > kWin32GlideSetterStateKeyWords)
        {
            // Truncating would collide distinct states into one key, so the call
            // is counted and excluded instead. A nonzero total means the target
            // list needs revisiting.
            RecordGlideSetterCensusKeyOverflow(census_, ordinal_);
            return;
        }
        // One key, built from the one generation counter that is live. The two
        // counters advance together in lockstep above.
        const std::uint32_t generation =
            cache_ != nullptr ? cache_->texture_generation
                              : (census_ != nullptr ? census_->texture_generation
                                                    : 0U);
        key_ = BuildGlideSetterStateKey(
            context_->glide_gate_stack + 1U,
            argument_words,
            IsGlideSetterStateTextureDependentGate(gate_id) ? generation : 0U);
        // Task 437: batch two widens the list, never the rules. The key, the
        // texture generation and the invalidation set are the same ones the
        // census measured the ceiling with.
        elision_candidate_ =
            cache_ != nullptr &&
            (IsGlideSetterElisionGate(gate_id) ||
             (GlideSetterTextureStateElisionEnabled() &&
              IsGlideSetterTextureStateElisionGate(gate_id)) ||
             (GlideSetterBatchThreeElisionEnabled() &&
              IsGlideSetterBatchThreeElisionGate(gate_id)) ||
             (GlideSetterBatchFourElisionEnabled() &&
              IsGlideSetterBatchFourElisionGate(gate_id)));
        handled_before_ = context_->glide_gate_handled_count;
        issues_before_ = TotalIssues();
        backend_failures_before_ = context_->glide_implementation_issues.total(
            repiu::hle::GlideImplementationIssueKind::kBackendFailure);
        active_ = true;
    }

    // Queried only after the return address, signature, and argument size have all
    // been validated, so a gate that would have been rejected is never elided.
    bool ShouldElide() const
    {
        return active_ && elision_candidate_ &&
            ShouldElideGlideSetterState(cache_, ordinal_, key_);
    }

    void MarkElided()
    {
        elided_ = true;
        RecordGlideSetterStateElided(cache_, ordinal_);
    }

    GlideSetterStateScope(const GlideSetterStateScope&) = delete;
    GlideSetterStateScope& operator=(const GlideSetterStateScope&) = delete;

  private:
    std::uint64_t TotalIssues() const
    {
        using kind = repiu::hle::GlideImplementationIssueKind;
        const auto& issues = context_->glide_implementation_issues;
        return issues.total(kind::kUnimplementedFunction) +
            issues.total(kind::kUnsupportedArgument) +
            issues.total(kind::kBackendFailure) +
            issues.total(kind::kAbiReject);
    }

    Win32GlideSetterCensusOutcome Classify() const
    {
        // A gate that never reached the handled path, and any backend failure,
        // leave the host state unknown. Anything else that recorded an issue was
        // retained for the ABI without the argument being expressed, so it is not
        // a successfully applied state either.
        if (context_->glide_gate_handled_count == handled_before_ ||
            context_->glide_implementation_issues.total(
                repiu::hle::GlideImplementationIssueKind::kBackendFailure) !=
                backend_failures_before_)
        {
            return Win32GlideSetterCensusOutcome::kFailed;
        }
        if (TotalIssues() != issues_before_)
        {
            return Win32GlideSetterCensusOutcome::kUnsupported;
        }
        return Win32GlideSetterCensusOutcome::kApplied;
    }

    Win32GlideSetterCensusProfile* census_ = nullptr;
    Win32GlideSetterStateCache* cache_ = nullptr;
    const ThreadContext* context_ = nullptr;
    Win32GlideSetterStateKey key_;
    std::uint16_t ordinal_ = 0;
    std::uint32_t handled_before_ = 0;
    std::uint64_t issues_before_ = 0;
    std::uint64_t backend_failures_before_ = 0;
    bool elision_candidate_ = false;
    bool elided_ = false;
    bool active_ = false;
};

// Task 438: hands the pending primitives to the backend in one rendezvous.
// Flushing an empty batch is a no-op success, which matters because the caller
// flushes unconditionally before every non-draw gate.
bool FlushGlideDrawBatchToBackend(ThreadContext* context,
                                  Win32GlideDrawBatchFlushReason reason)
{
    // Task 440: with the asynchronous present on, the flush is posted rather
    // than waited on. It has to be: left synchronous it would block the guest at
    // the next frame's first `grTexSource`, and the swap's wait would simply
    // move there instead of being returned to the guest.
    const bool asynchronous = GlideAsyncPresentEnabled();
    return FlushGlideDrawBatch(
        &context->glide_draw_batch, reason,
        [context, asynchronous](const repiu::hle::GlideDrawVertex* vertices,
                                std::size_t vertex_count,
                                Win32GlideBatchPrimitive primitive) {
            if (asynchronous)
            {
                return context->glide_backend.PostDrawPrimitiveBatch(
                    vertices, vertex_count, primitive);
            }
            return context->glide_backend.DrawPrimitiveBatch(
                vertices, vertex_count, primitive);
        });
}

// Task 476: grLfbReadRegion / grLfbWriteRegion are the only gates that use the
// staging surface as a live shadow of the frame buffer. `Ensure` fills it from
// the frame buffer once per burst, `Flush` presents accumulated region writes,
// and `Invalidate` forces the next burst to re-read. Doing this per row instead
// would mean 128 full-screen round trips per frame for the observed PIU pass.
bool IsGlideLfbRegionGate(repiu::hle::GlideGateId gate_id)
{
    return gate_id == repiu::hle::GlideGateId::kGrLfbReadRegion ||
        gate_id == repiu::hle::GlideGateId::kGrLfbWriteRegion;
}

// Bytes the guest side of a region transfer spans, or 0 when the arguments
// cannot describe a transfer this layer performs. The bound keeps a nonsense
// width or height from turning into an allocation.
std::size_t ResolveGlideLfbRegionSpan(std::uint32_t width,
                                      std::uint32_t height,
                                      std::uint32_t src_format,
                                      std::int32_t stride)
{
    constexpr std::uint64_t kMaximumRegionBytes = 64ULL * 1024ULL * 1024ULL;
    const std::uint32_t bytes_per_pixel =
        repiu::hle::GlideLfbSrcFormatBytesPerPixel(src_format);
    if (width == 0U || height == 0U || bytes_per_pixel == 0U || stride < 0)
    {
        return 0U;
    }
    const std::uint64_t row_pitch =
        static_cast<std::uint64_t>(repiu::hle::ResolveGlideLfbRegionStride(
            width, bytes_per_pixel, stride));
    const std::uint64_t span = (static_cast<std::uint64_t>(height) - 1ULL) *
            row_pitch +
        static_cast<std::uint64_t>(width) * bytes_per_pixel;
    if (span == 0ULL || span > kMaximumRegionBytes)
    {
        return 0U;
    }
    return static_cast<std::size_t>(span);
}

// Names the first reason a region transfer cannot be serviced, or null when the
// arguments are ones both region gates accept.
const char* ClassifyGlideLfbRegionArguments(const ThreadContext* context,
                                            std::uint32_t buffer,
                                            std::uint32_t width,
                                            std::uint32_t height,
                                            std::int32_t stride,
                                            std::uint32_t src_format)
{
    // The staging surface is the shadow and the lock buffer both. Servicing a
    // region transfer while the guest holds the lock pointer would rewrite
    // memory it is in the middle of using.
    if (context->glide_lfb_surface.locked())
    {
        return "a frame buffer lock is outstanding";
    }
    if (buffer != repiu::hle::kGlideBufferBackBuffer &&
        buffer != repiu::hle::kGlideBufferFrontBuffer)
    {
        return "only the front and back color buffers are implemented";
    }
    if (width == 0U || height == 0U)
    {
        return "region has no pixels";
    }
    if (stride < 0)
    {
        return "bottom-up images with a negative stride are not implemented";
    }
    if (!repiu::hle::GlideLfbSrcFormatSupported(src_format))
    {
        return "source pixel format is not implemented";
    }
    return nullptr;
}

bool FlushGlideLfbRegionShadow(ThreadContext* context);

bool EnsureGlideLfbRegionShadow(ThreadContext* context, std::uint32_t buffer)
{
    const std::uint32_t width = context->glide_state.width;
    const std::uint32_t height = context->glide_state.height;
    if (!context->glide_lfb_surface.Resize(width, height))
    {
        return false;
    }
    if (context->glide_lfb_region_shadow_valid &&
        context->glide_lfb_region_shadow_buffer != buffer)
    {
        FlushGlideLfbRegionShadow(context);
        context->glide_lfb_region_shadow_valid = false;
    }
    context->glide_lfb_region_shadow_buffer = buffer;
    if (context->glide_lfb_region_shadow_valid)
    {
        return true;
    }
    std::vector<std::uint8_t> rgba8;
    if (!context->glide_backend.ReadbackFramebuffer(width, height, &rgba8) ||
        !repiu::hle::EncodeRgba8ToGlideLfb565(
            rgba8.data(), rgba8.size(), width, height,
            context->glide_state.color_format,
            context->glide_lfb_surface.pixels(),
            context->glide_lfb_surface.byte_count()))
    {
        context->glide_backend_message = context->glide_backend.message();
        return false;
    }
    context->glide_lfb_region_shadow_valid = true;
    ++context->glide_lfb_region_seed_count;
    return true;
}

bool FlushGlideLfbRegionShadow(ThreadContext* context)
{
    if (!context->glide_lfb_region_shadow_dirty)
    {
        return true;
    }
    context->glide_lfb_region_shadow_dirty = false;
    std::vector<std::uint8_t> rgba8;
    if (!repiu::hle::DecodeGlideLfb565ToRgba8(
            context->glide_lfb_surface.pixels(),
            context->glide_lfb_surface.byte_count(),
            context->glide_lfb_surface.width(),
            context->glide_lfb_surface.height(),
            context->glide_state.color_format, &rgba8))
    {
        return false;
    }
    // `flip_v` is false because the shadow is top-down and stays that way:
    // ReadbackFramebuffer returns top-down rows and region coordinates are
    // native frame buffer rows, not origin-relative ones. PresentLfbSurface
    // XORs this against the window projection, so false is what lands row 0 at
    // the top of the screen under either origin.
    const bool present_to_front = context->glide_lfb_region_shadow_buffer ==
        repiu::hle::kGlideBufferFrontBuffer;
    if (!context->glide_backend.PresentLfbSurface(
            rgba8.data(), context->glide_lfb_surface.width(),
            context->glide_lfb_surface.height(), false, present_to_front))
    {
        context->glide_backend_message = context->glide_backend.message();
        return false;
    }
    ++context->glide_lfb_region_flush_count;
    ++context->glide_lfb_present_count;
    return true;
}

// Appends one primitive, flushing first when the batch cannot take it. Returns
// false only when the primitive was not queued at all, in which case the caller
// draws it directly and ordering is still preserved -- the batch is empty by
// then either way.
bool QueueGlideDrawForBatch(ThreadContext* context,
                            Win32GlideDrawBatch* batch,
                            const repiu::hle::GlideDrawVertex* vertices,
                            std::size_t vertex_count,
                            Win32GlideBatchPrimitive primitive)
{
    if (batch == nullptr)
    {
        return false;
    }
    bool flush_required = false;
    if (QueueGlideDrawPrimitive(batch, vertices, vertex_count, primitive,
                                &flush_required))
    {
        return true;
    }
    if (!flush_required)
    {
        return false;
    }
    const Win32GlideDrawBatchFlushReason reason =
        batch->primitive != primitive
            ? Win32GlideDrawBatchFlushReason::kPrimitiveChange
            : Win32GlideDrawBatchFlushReason::kCapacity;
    FlushGlideDrawBatchToBackend(context, reason);
    return QueueGlideDrawPrimitive(batch, vertices, vertex_count, primitive,
                                   &flush_required);
}

std::array<std::uint32_t,
           repiu::hle::kGlideImplementationIssueArgumentCapacity>
CaptureGlideImplementationIssueArguments(
    const CONTEXT* win32_context,
    ThreadContext* context,
    const std::uint32_t argument_byte_count)
{
    std::array<std::uint32_t,
               repiu::hle::kGlideImplementationIssueArgumentCapacity>
        arguments{};
    const std::size_t argument_count = std::min<std::size_t>(
        argument_byte_count / sizeof(std::uint32_t), arguments.size());
    const auto* guest_arguments = reinterpret_cast<const std::uint32_t*>(
        static_cast<std::uintptr_t>(win32_context->Esp) +
        sizeof(std::uint32_t));
    if (argument_count != 0U &&
        IsGuestRangeReadable(context,
                             guest_arguments,
                             argument_count * sizeof(std::uint32_t)))
    {
        std::memcpy(arguments.data(),
                    guest_arguments,
                    argument_count * sizeof(std::uint32_t));
        return arguments;
    }

    const std::size_t mirrored_count =
        std::min<std::size_t>(argument_count, 7U);
    std::copy_n(context->glide_gate_stack + 1U,
                mirrored_count,
                arguments.begin());
    return arguments;
}

void RecordGlideImplementationIssue(
    const CONTEXT* win32_context,
    ThreadContext* context,
    const repiu::hle::GlideExportGate& glide_export,
    const repiu::hle::GlideImplementationIssueKind kind,
    const std::string_view reason,
    const std::string_view detail,
    const char* action)
{
    const auto arguments = CaptureGlideImplementationIssueArguments(
        win32_context, context, glide_export.argument_byte_count);
    const auto result = context->glide_implementation_issues.Record(
        kind,
        glide_export.ordinal,
        glide_export.name,
        reason,
        detail,
        glide_export.argument_byte_count,
        arguments);
    if (result == repiu::hle::GlideImplementationIssueRecordResult::kRepeated)
    {
        return;
    }

    const bool fatal = repiu::hle::IsGlideImplementationIssueFatal(kind);
    if (result == repiu::hle::GlideImplementationIssueRecordResult::kOverflow)
    {
        if (context->glide_implementation_issues.overflow_count() == 1U)
        {
            fprintf(stderr,
                    "%s GLIDE_ISSUE_RECORD_OVERFLOW action=continue"
                    " capacity=%zu\n",
                    fatal ? "[repiu-fatal]" : "[repiu-error]",
                    repiu::hle::kGlideImplementationIssueRecordCapacity);
        }
        return;
    }

    const std::string line = repiu::hle::FormatGlideImplementationIssue(
        context->glide_implementation_issues.observations().back(),
        action);
    fprintf(stderr,
            "%s %s\n",
            fatal ? "[repiu-fatal]" : "[repiu-error]",
            line.c_str());
}

void RecordGlideTextureGateTrace(ThreadContext* context, const CONTEXT* win32_context, const repiu::hle::GlideExportGate& glide_export, std::uint32_t return_address, std::uint32_t return_eax, bool is_max_address)
{
    const std::uint32_t sequence = context->glide_texture_gate_trace_count + 1U;
    Win32GlideTextureGateTraceEntry& entry = context->glide_texture_gate_trace[(sequence - 1U) % kWin32GlideTextureGateTraceCapacity];
    entry.valid = true;
    entry.sequence = sequence;
    entry.ordinal = glide_export.ordinal;
    entry.is_max_address = is_max_address;
    entry.entry_eip = static_cast<std::uint32_t>(win32_context->Eip);
    entry.entry_esp = static_cast<std::uint32_t>(win32_context->Esp);
    entry.return_address = return_address;
    entry.tmu = context->glide_gate_stack[1];
    entry.entry_eax = static_cast<std::uint32_t>(win32_context->Eax);
    entry.return_eax = return_eax;
    entry.planned_return_esp = entry.entry_esp + 2U * sizeof(std::uint32_t);
    context->glide_texture_gate_trace_count = sequence;
    if (sequence > kWin32GlideTextureGateTraceCapacity)
    {
        context->glide_texture_gate_trace_wrapped = true;
    }
}

// Task 332 frame dump. Enabled by REPIU_GLIDE_FRAME_DUMP, which names how many
// swaps to skip between dumped frames; every draw of a dumped frame is logged.
bool Win32GlideFrameDumpEnabled()
{
    static const bool enabled =
        std::getenv("REPIU_GLIDE_FRAME_DUMP") != nullptr;
    return enabled;
}

long g_frame_dump_swap_index = 0;
long g_frame_dump_frames_done = 0;
bool g_frame_dump_active = false;

void Win32GlideAdvanceFrameDump()
{
    constexpr long kMaxDumpedFrames = 4;
    const char* value = std::getenv("REPIU_GLIDE_FRAME_DUMP");
    long interval = value != nullptr ? std::atol(value) : 0;
    if (interval <= 0)
    {
        interval = 60;
    }
    ++g_frame_dump_swap_index;
    if (g_frame_dump_active)
    {
        g_frame_dump_active = false;
        ++g_frame_dump_frames_done;
    }
    if (g_frame_dump_frames_done < kMaxDumpedFrames &&
        g_frame_dump_swap_index % interval == 0)
    {
        g_frame_dump_active = true;
        fprintf(stderr, "[repiu-frame-dump] begin swap=%ld\n",
                g_frame_dump_swap_index);
    }
}

// Task 375: this used to serve the texture download path as well, which meant a
// second decode of every texture purely for the dump and a 24-bit file that
// dropped alpha -- the one channel the sprite investigation needed. Textures now
// dump through the backend's TGA writer, and this remains for the LFB surface
// alone.
void DumpLfbSurfaceToBmp(std::uint32_t start_address, std::uint32_t format, std::uint32_t width, std::uint32_t height, const std::vector<std::uint8_t>& rgba)
{
    static std::uint32_t s_dump_counter = 0;
    try
    {
        std::filesystem::path dump_dir = "build/texture_dumps";
        std::filesystem::create_directories(dump_dir);

        std::ostringstream filename_stream;
        filename_stream << "tex_0x" << std::uppercase << std::hex << start_address
                        << "_fmt" << std::dec << format
                        << "_" << width << "x" << height
                        << "_" << ++s_dump_counter << ".bmp";

        std::filesystem::path filepath = dump_dir / filename_stream.str();
        std::ofstream file(filepath, std::ios::binary);
        if (!file)
        {
            return;
        }

        #pragma pack(push, 1)
        struct BmpFileHeader
        {
            std::uint16_t bfType = 0x4D42;
            std::uint32_t bfSize = 0;
            std::uint16_t bfReserved1 = 0;
            std::uint16_t bfReserved2 = 0;
            std::uint32_t bfOffBits = 54;
        };

        struct BmpInfoHeader
        {
            std::uint32_t biSize = 40;
            std::int32_t biWidth = 0;
            std::int32_t biHeight = 0;
            std::uint16_t biPlanes = 1;
            std::uint16_t biBitCount = 32;
            std::uint32_t biCompression = 0;
            std::uint32_t biSizeImage = 0;
            std::int32_t biXPelsPerMeter = 3780;
            std::int32_t biYPelsPerMeter = 3780;
            std::uint32_t biClrUsed = 0;
            std::uint32_t biClrImportant = 0;
        };
        #pragma pack(pop)

        // Write 24-bit bottom-up BI_RGB, the one BMP form every viewer handles.
        // The previous 32-bit top-down form is legal but fragile: the fourth
        // byte of a 32-bit BI_RGB pixel is officially reserved, so viewers
        // disagree on whether it is alpha, and negative biHeight is uncommon
        // enough that some refuse the file outright -- which reads as "the dump
        // never happened" even though the bytes are correct.
        const std::size_t row_padding = (4U - ((width * 3U) % 4U)) % 4U;
        const std::size_t row_bytes = width * 3U + row_padding;
        std::vector<std::uint8_t> bgr(row_bytes * height, 0U);
        for (std::uint32_t y = 0; y < height; ++y)
        {
            // Bottom-up: BMP row 0 is the image's last row.
            const std::uint32_t source_row = height - 1U - y;
            std::uint8_t* out = bgr.data() + static_cast<std::size_t>(y) *
                row_bytes;
            for (std::uint32_t x = 0; x < width; ++x)
            {
                const std::size_t i =
                    (static_cast<std::size_t>(source_row) * width + x) * 4U;
                out[x * 3U + 0U] = rgba[i + 2U]; // B
                out[x * 3U + 1U] = rgba[i + 1U]; // G
                out[x * 3U + 2U] = rgba[i + 0U]; // R
            }
        }

        BmpFileHeader file_header;
        BmpInfoHeader info_header;
        info_header.biBitCount = 24;
        info_header.biWidth = static_cast<std::int32_t>(width);
        info_header.biHeight = static_cast<std::int32_t>(height);
        info_header.biSizeImage = static_cast<std::uint32_t>(bgr.size());
        file_header.bfSize = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader) +
            info_header.biSizeImage;

        file.write(reinterpret_cast<const char*>(&file_header), sizeof(file_header));
        file.write(reinterpret_cast<const char*>(&info_header), sizeof(info_header));
        file.write(reinterpret_cast<const char*>(bgr.data()), bgr.size());

        // Alpha is where a texture "not showing" usually hides, and dropping it
        // from the colour dump would lose that. Emit it as a separate grayscale
        // image for formats that carry one.
        const bool has_alpha = format == 8U || format == 11U || format == 12U ||
            format == 13U || format == 14U || format == 2U || format == 4U;
        if (has_alpha)
        {
            std::filesystem::path alpha_path = dump_dir /
                (filename_stream.str().substr(
                     0, filename_stream.str().size() - 4U) + "_alpha.bmp");
            std::ofstream alpha_file(alpha_path, std::ios::binary);
            if (alpha_file)
            {
                std::vector<std::uint8_t> mono(row_bytes * height, 0U);
                for (std::uint32_t y = 0; y < height; ++y)
                {
                    const std::uint32_t source_row = height - 1U - y;
                    std::uint8_t* out = mono.data() +
                        static_cast<std::size_t>(y) * row_bytes;
                    for (std::uint32_t x = 0; x < width; ++x)
                    {
                        const std::uint8_t a = rgba[(static_cast<std::size_t>(
                            source_row) * width + x) * 4U + 3U];
                        out[x * 3U + 0U] = a;
                        out[x * 3U + 1U] = a;
                        out[x * 3U + 2U] = a;
                    }
                }
                alpha_file.write(reinterpret_cast<const char*>(&file_header),
                                 sizeof(file_header));
                alpha_file.write(reinterpret_cast<const char*>(&info_header),
                                 sizeof(info_header));
                alpha_file.write(reinterpret_cast<const char*>(mono.data()),
                                 mono.size());
            }
        }
    }
    catch (...)
    {
        // Fail-safe to avoid crashing the loader on diagnostics
    }
}

} // namespace

void RecordAllocatorControlFlowException(
    EXCEPTION_POINTERS* exception_info,
    ThreadContext* context)
{
    if (exception_info == nullptr || context == nullptr ||
        exception_info->ContextRecord == nullptr)
    {
        return;
    }

    const std::uint32_t eip = static_cast<std::uint32_t>(
        exception_info->ContextRecord->Eip);
    const std::uint64_t runtime_end =
        static_cast<std::uint64_t>(context->runtime_base) +
        context->runtime_size;
    if (eip < context->runtime_base ||
        static_cast<std::uint64_t>(eip) + 4U > runtime_end)
    {
        return;
    }

    const std::uint32_t eip_offset = eip - context->runtime_base;
    constexpr std::uint32_t kAllocatorTraceBegin = 0x000F7A60U;
    constexpr std::uint32_t kAllocatorTraceEnd = 0x000F7AD5U;
    if (eip_offset < kAllocatorTraceBegin ||
        eip_offset >= kAllocatorTraceEnd)
    {
        return;
    }

    Win32AllocatorControlFlowObservation& observation =
        context->allocator_control_flow;
    const std::uint32_t sequence = observation.observed_count + 1;
    const std::uint32_t slot =
        (sequence - 1) % kWin32AllocatorControlFlowTraceCapacity;
    Win32AllocatorControlFlowTraceEntry& entry = observation.trace[slot];
    entry.valid = true;
    entry.sequence = sequence;
    entry.eip_offset = eip_offset;
    entry.seh_code = exception_info->ExceptionRecord != nullptr
        ? exception_info->ExceptionRecord->ExceptionCode
        : 0;
    const std::uint8_t* instruction =
        reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(eip));
    std::memcpy(entry.opcode, instruction, sizeof(entry.opcode));
    entry.eax = exception_info->ContextRecord->Eax;
    entry.ebx = exception_info->ContextRecord->Ebx;
    entry.edx = exception_info->ContextRecord->Edx;
    entry.esi = exception_info->ContextRecord->Esi;
    entry.edi = exception_info->ContextRecord->Edi;
    entry.eflags = exception_info->ContextRecord->EFlags;
    entry.pending_valid = context->pending_shadow_allocation_valid;
    entry.pending_size = context->pending_shadow_allocation_size;
    observation.observed_count = sequence;
    if (observation.trace_stored_count <
        kWin32AllocatorControlFlowTraceCapacity)
    {
        ++observation.trace_stored_count;
    }
    else
    {
        observation.trace_wrapped = true;
    }
}

bool HandleLinexeFarTransferBoundary(CONTEXT* win32_context,
                                     ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr ||
        !context->linexe_environment_active)
    {
        return false;
    }

    const auto* instruction = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(win32_context->Eip));
    constexpr std::size_t kFarPointerSize = 6U;
    if (IsGuestRangeReadable(context, instruction, kFarPointerSize) &&
        instruction[0] == 0xFFU && instruction[1] == 0x1DU)
    {
        const std::uint32_t pointer_address =
            static_cast<std::uint32_t>(instruction[2]) |
            (static_cast<std::uint32_t>(instruction[3]) << 8U) |
            (static_cast<std::uint32_t>(instruction[4]) << 16U) |
            (static_cast<std::uint32_t>(instruction[5]) << 24U);
        const auto* pointer = reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(pointer_address));
        if (IsGuestRangeReadable(context, pointer, kFarPointerSize))
        {
            const std::uint32_t target_offset =
                static_cast<std::uint32_t>(pointer[0]) |
                (static_cast<std::uint32_t>(pointer[1]) << 8U) |
                (static_cast<std::uint32_t>(pointer[2]) << 16U) |
                (static_cast<std::uint32_t>(pointer[3]) << 24U);
            const std::uint16_t target_selector =
                static_cast<std::uint16_t>(pointer[4]) |
                static_cast<std::uint16_t>(pointer[5] << 8U);
            repiu::hle::LinexeService service{};
            ++context->linexe_indirect_far_call_count;
            context->linexe_indirect_far_call_source =
                static_cast<std::uint32_t>(win32_context->Eip);
            context->linexe_indirect_far_call_pointer = pointer_address;
            context->linexe_indirect_far_call_offset = target_offset;
            context->linexe_indirect_far_call_selector = target_selector;
            context->linexe_indirect_far_call_known_export =
                repiu::hle::DecodeLinexeOriginalExport(
                    context->linexe_gate_plan,
                    target_selector,
                    static_cast<std::uint16_t>(target_offset),
                    &service);
        }
        return false;
    }
    constexpr std::uint8_t kFarTransferPrefix[] =
        {0x66U, 0xEAU, 0x04U, 0x00U};
    if (!IsGuestRangeReadable(
            context, instruction, sizeof(kFarTransferPrefix) + 2U) ||
        std::memcmp(instruction,
                    kFarTransferPrefix,
                    sizeof(kFarTransferPrefix)) != 0)
    {
        return false;
    }

    const std::uint16_t target_selector = static_cast<std::uint16_t>(
        win32_context->Edi >> 16U);
    const std::uint16_t target_offset = static_cast<std::uint16_t>(
        win32_context->Edi & 0xFFFFU);
    repiu::hle::LinexeService service{};
    if (!repiu::hle::DecodeLinexeOriginalExport(
            context->linexe_gate_plan,
            target_selector,
            target_offset,
            &service))
    {
        return false;
    }

    ++context->linexe_bridge_entry_count;
    context->linexe_bridge_gate_valid = true;
    context->linexe_bridge_selector = target_selector;
    context->linexe_bridge_offset = target_offset;
    context->linexe_bridge_service = static_cast<std::uint32_t>(service);
    context->linexe_bridge_esp = win32_context->Esp;
    context->linexe_bridge_ebp = win32_context->Ebp;
    const auto* stack = reinterpret_cast<const std::uint32_t*>(
        static_cast<std::uintptr_t>(win32_context->Esp));
    if (IsGuestRangeReadable(context,
                             stack,
                             sizeof(context->linexe_bridge_stack)))
    {
        std::memcpy(context->linexe_bridge_stack,
                    stack,
                    sizeof(context->linexe_bridge_stack));
    }
    std::memset(context->linexe_bridge_argument_text,
                0,
                sizeof(context->linexe_bridge_argument_text));
    std::memset(context->linexe_bridge_stack_text,
                0,
                sizeof(context->linexe_bridge_stack_text));
    for (std::size_t stack_index = 0;
         stack_index < std::size(context->linexe_bridge_stack);
         ++stack_index)
    {
        const auto* candidate = reinterpret_cast<const char*>(
            static_cast<std::uintptr_t>(
                context->linexe_bridge_stack[stack_index]));
        std::size_t text_length = 0;
        for (; text_length + 1U <
                   sizeof(context->linexe_bridge_stack_text[stack_index]);
             ++text_length)
        {
            if (!IsGuestRangeReadable(context, candidate + text_length, 1U))
            {
                break;
            }
            const unsigned char value = static_cast<unsigned char>(
                candidate[text_length]);
            if (value == 0)
            {
                break;
            }
            if (!std::isprint(value))
            {
                text_length = 0;
                break;
            }
            context->linexe_bridge_stack_text[stack_index][text_length] =
                static_cast<char>(value);
        }
        if (text_length == 0)
        {
            context->linexe_bridge_stack_text[stack_index][0] = '\0';
        }
    }
    const auto* argument = reinterpret_cast<const char*>(
        static_cast<std::uintptr_t>(context->linexe_bridge_stack[9]));
    for (std::size_t index = 0;
         index + 1U < sizeof(context->linexe_bridge_argument_text);
         ++index)
    {
        if (!IsGuestRangeReadable(context, argument + index, 1U))
        {
            break;
        }
        context->linexe_bridge_argument_text[index] = argument[index];
        if (argument[index] == '\0')
        {
            break;
        }
    }

    constexpr std::uint32_t kVirtualGlideModuleHandle = 1U;
    // The bridge consumes its three dwords and restores the ES value saved by
    // the wrapper.  The shared epilogue then owns EBX/ESI/EDI/EBP and RET.
    const bool is_glide_module =
        _stricmp(context->linexe_bridge_argument_text, "glide2x.ovl") == 0;
    const repiu::hle::GlideExportGate* glide_export =
        repiu::hle::FindGlideExportByName(
            context->glide_gate_plan,
            context->linexe_bridge_stack_text[12]);
    if (service == repiu::hle::LinexeService::kGetProcedureAddress &&
        context->linexe_bridge_stack[11] == kVirtualGlideModuleHandle)
    {
        std::strncpy(context->linexe_get_proc_name,
                     context->linexe_bridge_stack_text[12],
                     sizeof(context->linexe_get_proc_name) - 1U);
    }    if (service == repiu::hle::LinexeService::kGetProcedureAddress &&
        context->linexe_bridge_stack[11] == kVirtualGlideModuleHandle &&
        glide_export != nullptr)
    {
        const std::uint32_t result_pointer =
            context->linexe_bridge_stack[13];
        const std::uint32_t gate_address =
            context->linexe_arena_layout.gate_code_base +
            glide_export->gate_offset;
        const std::uint32_t procedure_pointer[2] = {
            gate_address,
            static_cast<std::uint32_t>(win32_context->SegCs),
        };
        if (!WriteGuestBytes(
                context,
                reinterpret_cast<void*>(
                    static_cast<std::uintptr_t>(result_pointer)),
                procedure_pointer,
                sizeof(procedure_pointer)))
        {
            return false;
        }

        ++context->linexe_get_proc_count;
        context->linexe_get_proc_result_pointer = gate_address;
        std::strncpy(context->linexe_get_proc_name,
                     glide_export->name.c_str(),
                     sizeof(context->linexe_get_proc_name) - 1U);
        win32_context->Eax = 1U;
        context->guest_es = static_cast<std::uint16_t>(
            context->linexe_bridge_stack[5] & 0xFFFFU);
        ReResolveAotSegmentOverrides(context);
        win32_context->Ebx = context->linexe_bridge_stack[6];
        win32_context->Esi = context->linexe_bridge_stack[7];
        win32_context->Edi = context->linexe_bridge_stack[8];
        win32_context->Ebp = context->linexe_bridge_stack[9];
        win32_context->Eip = context->linexe_bridge_stack[10];
        win32_context->Esp += 11U * sizeof(std::uint32_t);
        return true;
    }
    if (service != repiu::hle::LinexeService::kLoadModule ||
        !is_glide_module)
    {
        return false;
    }

    ++context->linexe_virtual_module_load_count;
    context->linexe_virtual_module_handle = kVirtualGlideModuleHandle;
    win32_context->Eax = kVirtualGlideModuleHandle;
    context->guest_es = static_cast<std::uint16_t>(
        context->linexe_bridge_stack[3] & 0xFFFFU);
    ReResolveAotSegmentOverrides(context);
    win32_context->Ebx = context->linexe_bridge_stack[4];
    win32_context->Esi = context->linexe_bridge_stack[5];
    win32_context->Edi = context->linexe_bridge_stack[6];
    win32_context->Ebp = context->linexe_bridge_stack[7];
    win32_context->Eip = context->linexe_bridge_stack[8];
    win32_context->Esp += 9U * sizeof(std::uint32_t);
    return true;
}

bool HandleGlideGateBoundary(CONTEXT* win32_context,
                             ThreadContext* context)
{
    const std::uint32_t gate_begin =
        context != nullptr
            ? context->linexe_arena_layout.gate_code_base +
                context->glide_gate_plan.first_gate_offset
            : 0U;
    if (win32_context == nullptr || context == nullptr ||
        !context->linexe_environment_active ||
        win32_context->Eip < gate_begin)
    {
        return false;
    }

    // Task 323: measured after the cheap rejection so the bucket counts real
    // gate work only. This includes any wait on the SDL main-thread render
    // queue, which is exactly what the measurement needs to expose.
    std::uint64_t ordinal_gate_cycles = 0U;
    GlideOrdinalTimingScope ordinal_timing_scope(
        GlideOrdinalTimingProfileEnabled()
            ? &context->glide_ordinal_timing
            : nullptr,
        &context->glide_backend,
        &ordinal_gate_cycles);
    // Task 438: null when batching is off, which keeps the draw cases on exactly
    // the path they took before -- one rendezvous per primitive.
    Win32GlideDrawBatch* const draw_batch =
        GlideDrawBatchEnabled() ? &context->glide_draw_batch : nullptr;
    // Tasks 364/365: declared here so its destructor observes the dispatch outcome
    // on every return path, and begun below once the argument mirror is filled.
    GlideSetterStateScope setter_state_scope(
        GlideSetterCensusEnabled() ? &context->glide_setter_census : nullptr,
        GlideSetterElisionEnabled() ? &context->glide_setter_state_cache
                                    : nullptr,
        context);
    const ExecutionTimeScope gate_time_scope(
        context->execution_time_profile.get(),
        ExecutionTimeBucket::kGlideGate,
        &ordinal_gate_cycles);

    const std::uint32_t gate_offset =
        static_cast<std::uint32_t>(win32_context->Eip) -
        context->linexe_arena_layout.gate_code_base;
    const repiu::hle::GlideExportGate* glide_export =
        repiu::hle::DecodeGlideGate(context->glide_gate_plan, gate_offset);
    if (glide_export == nullptr)
    {
        return false;
    }

    using go = repiu::hle::GlideGateId;
    ordinal_timing_scope.Begin(glide_export->ordinal);

    ++context->glide_gate_entry_count;
    // Task 335: pumping here costs a full host-thread rendezvous — measured at
    // 1.92 rendezvous per gate entry, the second one being this call — while
    // the host poll loop already pumps every iteration, roughly every 0.68ms
    // since Task 333 and immediately whenever a command is posted. Events are
    // still processed only on the host thread; only the redundant caller goes.
    // `REPIU_GLIDE_GATE_PUMP=1` restores it for A/B.
    if (GlideGatePumpEventsEnabled())
    {
        context->glide_backend.PumpEvents();
    }
    context->glide_gate_ordinal = glide_export->ordinal;
    context->glide_gate_argument_bytes = glide_export->argument_byte_count;
    std::memset(context->glide_gate_name,
                0,
                sizeof(context->glide_gate_name));
    std::strncpy(context->glide_gate_name,
                 glide_export->name.c_str(),
                 sizeof(context->glide_gate_name) - 1U);
    context->glide_gate_esp = win32_context->Esp;
    const auto* stack = reinterpret_cast<const std::uint32_t*>(
        static_cast<std::uintptr_t>(win32_context->Esp));
    if (IsGuestRangeReadable(context,
                             stack,
                             sizeof(context->glide_gate_stack)))
    {
        std::memcpy(context->glide_gate_stack,
                    stack,
                    sizeof(context->glide_gate_stack));
    }
    setter_state_scope.Begin(*glide_export);
    if (glide_export->ordinal < context->glide_call_counts.size())
    {
        const std::size_t ordinal = glide_export->ordinal;
        if (context->glide_call_counts[ordinal]++ == 0U)
        {
            context->glide_call_names[ordinal] = glide_export->name;
            std::copy(std::begin(context->glide_gate_stack),
                      std::end(context->glide_gate_stack),
                      context->glide_first_stacks[ordinal].begin());
            // Audit diagnostic (env-gated, off by default): emit one line the
            // first time each ordinal is ever called. Bounded by the export
            // count (<=97 lines), so it survives both the 96-entry gate-entry
            // log cap and the timeout path that skips the exit summary --
            // the only way to enumerate the reached API set on a timed run.
            static const bool call_audit_enabled =
                std::getenv("REPIU_GLIDE_CALL_AUDIT") != nullptr;
            if (call_audit_enabled)
            {
                fprintf(stderr,
                        "[repiu-glide-audit] first-call ordinal=%u name=%s"
                        " args=%08X %08X %08X %08X %08X %08X %08X\n",
                        glide_export->ordinal, glide_export->name.c_str(),
                        context->glide_gate_stack[1],
                        context->glide_gate_stack[2],
                        context->glide_gate_stack[3],
                        context->glide_gate_stack[4],
                        context->glide_gate_stack[5],
                        context->glide_gate_stack[6],
                        context->glide_gate_stack[7]);
            }
            if (context->shared_live_telemetry != nullptr)
            {
                using go = repiu::hle::GlideGateId;
                volatile long* milestone = nullptr;
                const repiu::hle::GlideGateId gate_id = glide_export->gate_id;
                if (gate_id == go::kGrSstWinOpen)
                {
                    milestone = &context->shared_live_telemetry
                        ->glide_window_gate_milestone;
                }
                else if (gate_id == go::kGrTexDownloadMipMapLevel)
                {
                    milestone = &context->shared_live_telemetry
                        ->glide_texture_milestone;
                }
                // Task 420: the antialiased entry points draw too now, and they
                // sit outside this contiguous block, so they are named here
                // rather than left out of the draw milestone.
                else if ((gate_id >= go::kGrDrawLine &&
                          gate_id <= go::kGrDrawPolygon) ||
                         (gate_id >= go::kGrAADrawPoint &&
                          gate_id <= go::kGrDrawPolygonVertexList))
                {
                    milestone = &context->shared_live_telemetry
                        ->glide_draw_milestone;
                }
                else if (gate_id == go::kGrBufferSwap)
                {
                    milestone = &context->shared_live_telemetry
                        ->glide_swap_milestone;
                }
                if (milestone != nullptr)
                {
                    InterlockedExchange(milestone, 1L);
                }
            }
        }
    }
    // Task 255 R3 observation (env-gated, off by default): dump the real
    // arguments of the texture/combine gates during content draws so the actual
    // format, dimensions, and combine mode can be confirmed before implementing
    // texture decode/upload/sampling. The gate stack mirror holds only 8 dwords,
    // so read directly from the guest stack for the wider download call.
    {
        using go = repiu::hle::GlideGateId;
        static const bool tex_diagnostic_enabled =
            std::getenv("REPIU_GLIDE_TEX_DIAG") != nullptr;
        const repiu::hle::GlideGateId gate_id = glide_export->gate_id;
        if (tex_diagnostic_enabled &&
            (gate_id == go::kGrTexDownloadMipMapLevel ||
             gate_id == go::kGrTexSource ||
             gate_id == go::kGrTexCombine ||
             gate_id == go::kGrColorCombine ||
             gate_id == go::kGrAlphaBlendFunction ||
             gate_id == go::kGrAlphaTestFunction ||
             gate_id == go::kGrAlphaCombine))
        {
            static long tex_diag_count = 0;
            const long diag_index = InterlockedIncrement(&tex_diag_count);
            if (diag_index <= 256)
            {
                std::uint32_t args[9] = {};
                const auto* guest_stack =
                    reinterpret_cast<const std::uint32_t*>(
                        static_cast<std::uintptr_t>(win32_context->Esp));
                if (IsGuestRangeReadable(context, guest_stack, sizeof(args)))
                {
                    std::memcpy(args, guest_stack, sizeof(args));
                }
                fprintf(stderr,
                        "[repiu-live-debug] tex-diag #%ld %s args=%08X %08X %08X"
                        " %08X %08X %08X %08X %08X\n",
                        diag_index, glide_export->name.c_str(),
                        args[1], args[2], args[3], args[4],
                        args[5], args[6], args[7], args[8]);
            }
        }
    }
    // Task 246: gate traffic is tiny (tens of calls), so log every entry to
    // pin which call leaks its stack frame (an unhandled entry resumes the
    // caller without the stdcall cleanup, offsetting ESP for the rest of the
    // frame — the confirmed zero-EIP mechanism at 0x0304ED35).
    {
        static long gate_entry_log_count = 0;
        const long entry_index = InterlockedIncrement(&gate_entry_log_count);
        if (entry_index <= 96)
        {
            fprintf(stderr,
                    "[repiu-live-debug] glide gate entry #%ld ordinal=%u"
                    " name=%s ret=0x%08X esp=0x%08X\n",
                    entry_index, glide_export->ordinal,
                    glide_export->name.c_str(), context->glide_gate_stack[0],
                    static_cast<std::uint32_t>(win32_context->Esp));
        }
    }
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->glide_gate_ordinal,
            static_cast<long>(glide_export->ordinal));
        InterlockedExchange(
            &context->shared_live_telemetry->glide_gate_esp,
            static_cast<long>(win32_context->Esp));
        InterlockedExchange(
            &context->shared_live_telemetry->glide_gate_ebx,
            static_cast<long>(win32_context->Ebx));
        InterlockedExchange(
            &context->shared_live_telemetry->glide_gate_ecx,
            static_cast<long>(win32_context->Ecx));
        InterlockedExchange(
            &context->shared_live_telemetry->glide_gate_edx,
            static_cast<long>(win32_context->Edx));
        for (std::size_t index = 0; index < 8U; ++index)
        {
            InterlockedExchange(
                &context->shared_live_telemetry->glide_gate_stack[index],
                static_cast<long>(context->glide_gate_stack[index]));
        }
    }
    const std::uint32_t return_address = context->glide_gate_stack[0];
    if (!IsGuestInstructionPointer(context, return_address))
    {
        RecordGlideImplementationIssue(
            win32_context,
            context,
            *glide_export,
            repiu::hle::GlideImplementationIssueKind::kAbiReject,
            "return-address-not-guest",
            {},
            "terminate");
        return false;
    }
    const repiu::hle::GlideSignature* signature =
        repiu::hle::FindGlideSignature(glide_export->name);
    if (signature == nullptr)
    {
        RecordGlideImplementationIssue(
            win32_context,
            context,
            *glide_export,
            repiu::hle::GlideImplementationIssueKind::
                kUnimplementedFunction,
            "signature-not-cataloged",
            {},
            "terminate");
        return false;
    }
    if (signature->argument_byte_count !=
        glide_export->argument_byte_count)
    {
        const std::string detail =
            "catalog-argument-bytes=" +
            std::to_string(signature->argument_byte_count);
        RecordGlideImplementationIssue(
            win32_context,
            context,
            *glide_export,
            repiu::hle::GlideImplementationIssueKind::kAbiReject,
            "signature-mismatch",
            detail,
            "terminate");
        return false;
    }
    // Task 365: an exact repeat of a state already applied successfully on the
    // host skips only the `InvokeOnHostThread` round trip. Everything the case
    // body would have done that the guest can observe still happens here: the
    // handled count, the return address, and the stdcall cleanup. The
    // `glide_state` mirror write is skipped safely because it is idempotent --
    // the application this key matches already wrote the identical values, and
    // the key holds every argument dword.
    //
    // Placed after the return-address, signature, and argument-size checks so a
    // gate that would have been rejected is never elided. `message_` is left
    // alone: it is host-owned and writing it from the guest thread would race,
    // and the message from the matching application is still accurate.
    //
    // Batch one is void-returning throughout, so `Eax` is untouched.
    if (setter_state_scope.ShouldElide())
    {
        setter_state_scope.MarkElided();
        ++context->glide_gate_handled_count;
        win32_context->Eip = return_address;
        win32_context->Esp +=
            sizeof(std::uint32_t) + signature->argument_byte_count;
        return true;
    }
    const auto decline_gate = [context, win32_context, glide_export, signature,
                               return_address](const char* reason) {
        const std::string_view reason_view(reason);
        const bool backend_failure =
            reason_view.find("backend-failure") != std::string_view::npos;
        const std::string_view detail =
            backend_failure ? std::string_view(context->glide_backend_message)
                            : std::string_view{};
        const bool unsupported_argument =
            backend_failure && detail.starts_with("unsupported Glide");
        RecordGlideImplementationIssue(
            win32_context,
            context,
            *glide_export,
            unsupported_argument
                ? repiu::hle::GlideImplementationIssueKind::
                      kUnsupportedArgument
                : repiu::hle::GlideImplementationIssueKind::kBackendFailure,
            reason_view,
            detail,
            "continue");
        ++context->glide_gate_handled_count;
        if (signature->return_kind != repiu::hle::GlideReturnKind::kVoid)
        {
            win32_context->Eax = 0U;
        }
        win32_context->Eip = return_address;
        win32_context->Esp +=
            sizeof(std::uint32_t) + signature->argument_byte_count;
        return true;
    };
    // Task 420: the A/B switch for the newly implemented draw entry points.
    // When disabled, each of those cases records the issue it used to record
    // and returns without drawing, keeping the stack adjustment identical, so
    // one binary can answer whether drawing them is what changed a run.
    // `REPIU_GLIDE_DRAW_ENTRY_POINTS=0` restores the old behaviour.
    static const bool draw_entry_points_enabled = [] {
        const char* value = std::getenv("REPIU_GLIDE_DRAW_ENTRY_POINTS");
        return value == nullptr || std::strcmp(value, "0") != 0;
    }();
    const auto record_unimplemented =
        [context, win32_context, glide_export](
            const std::string_view reason,
            const std::string_view detail) {
            RecordGlideImplementationIssue(
                win32_context,
                context,
                *glide_export,
                repiu::hle::GlideImplementationIssueKind::
                    kUnimplementedFunction,
                reason,
                detail,
                "continue");
        };
    const auto record_unsupported =
        [context, win32_context, glide_export](
            const std::string_view reason,
            const std::string_view detail) {
            RecordGlideImplementationIssue(
                win32_context,
                context,
                *glide_export,
                repiu::hle::GlideImplementationIssueKind::
                    kUnsupportedArgument,
                reason,
                detail,
                "continue");
        };
    const auto record_backend_failure =
        [context, win32_context, glide_export](
            const std::string_view reason,
            const std::string_view detail) {
            RecordGlideImplementationIssue(
                win32_context,
                context,
                *glide_export,
                repiu::hle::GlideImplementationIssueKind::kBackendFailure,
                reason,
                detail,
                "continue");
        };
    using go = repiu::hle::GlideGateId;
    // Task 438: the whole ordering contract, in one rule. Anything that is not a
    // queueable draw flushes first, so state changes, queries, swap, clear, LFB
    // and downloads all see every triangle that preceded them without any of
    // them having to be enumerated -- an omission in such a list would be an
    // ordering bug that only shows up as a wrong picture.
    //
    // Placed after the elision short-circuit above on purpose: an elided setter
    // returns before this point, changes nothing, and therefore must not force
    // a flush. That is what leaves batches long enough to be worth having.
    //
    // Task 476: the LFB region shadow follows the same rule for the same
    // reason. It goes first so pixels reach the frame buffer before anything
    // else touches it, and it invalidates unconditionally because any other
    // gate -- a draw, a clear, a swap -- may change what the shadow claims to
    // mirror. Region gates are non-draw gates, so the batch is always empty
    // while the shadow is dirty and the two flushes cannot reorder each other.
    if (!IsGlideLfbRegionGate(glide_export->gate_id) &&
        (context->glide_lfb_region_shadow_valid ||
         context->glide_lfb_region_shadow_dirty))
    {
        FlushGlideLfbRegionShadow(context);
        context->glide_lfb_region_shadow_valid = false;
    }
    if (draw_batch != nullptr && !IsGlideDrawBatchGate(glide_export->gate_id))
    {
        FlushGlideDrawBatchToBackend(
            context, Win32GlideDrawBatchFlushReason::kNonDrawGate);
    }
    // 100% Unified GateId O(1) Switch Dispatcher (Task 321)
    switch (glide_export->gate_id)
    {
        case go::kGrHints: // _GRHINTS@8
        {
            // grHints(GrHint_t type, FxU32 mask) declares driver optimizations,
            // not rendering state. STWHINT says which w and s/t values are
            // unique per TMU, FIFOCHECKHINT how often Glide polls the command
            // FIFO, FPUPRECISION what precision Glide may use for its own math,
            // and ALLOW_MIPMAP_DITHER whether mipmap dithering is permitted.
            //
            // The `GrVertex` layout is fixed by the ABI, so a renderer that
            // reads the structure directly draws identically whatever the hint
            // says. Recording the declaration is therefore a complete
            // implementation for this backend rather than a stub, which is why
            // this no longer reports an implementation gap. An unknown hint
            // type still does, because that would be a real gap.
            const std::uint32_t hint_type = context->glide_gate_stack[1];
            const std::uint32_t hint_mask = context->glide_gate_stack[2];
            constexpr std::uint32_t kHintStwHint = 0U;
            constexpr std::uint32_t kHintFifoCheckHint = 1U;
            constexpr std::uint32_t kHintFpuPrecision = 2U;
            constexpr std::uint32_t kHintAllowMipmapDither = 3U;
            // Bits above the three-TMU set are not defined by Glide 2.x.
            constexpr std::uint32_t kStwHintMask = 0x7FU;
            switch (hint_type)
            {
                case kHintStwHint:
                    if ((hint_mask & ~kStwHintMask) != 0U)
                    {
                        RecordGlideImplementationIssue(
                            win32_context, context, *glide_export,
                            repiu::hle::GlideImplementationIssueKind::
                                kUnsupportedArgument,
                            "stw-hint-reserved-bits",
                            "STW hint carries bits outside the Glide 2.x set",
                            "continue");
                    }
                    context->glide_state.stw_hint = hint_mask;
                    break;
                case kHintFifoCheckHint:
                    context->glide_state.fifo_check_hint = hint_mask;
                    break;
                case kHintFpuPrecision:
                    context->glide_state.fpu_precision_hint = hint_mask;
                    break;
                case kHintAllowMipmapDither:
                    context->glide_state.allow_mipmap_dither_hint = hint_mask;
                    break;
                default:
                    RecordGlideImplementationIssue(
                        win32_context, context, *glide_export,
                        repiu::hle::GlideImplementationIssueKind::
                            kUnsupportedArgument,
                        "unknown-hint-type",
                        "hint type is outside GR_HINTTYPE_MIN..MAX",
                        "continue");
                    break;
            }
            context->glide_state.hints_seen = true;
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 3U * sizeof(std::uint32_t);
            return true;
        }
        case go::kGrGlideInit: // _GRGLIDEINIT@0
            context->glide_state.initialized = true;
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += sizeof(std::uint32_t);
            return true;

        case go::kGrSstQueryHardware: // _GRSSTQUERYHARDWARE@4
        {
            void* configuration = reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(context->glide_gate_stack[1]));
            if (!WriteGuestUInt32(context, configuration, 1U))
            {
                return decline_gate("query-hardware-unwritable-memory");
            }
            ++context->glide_gate_handled_count;
            win32_context->Eax = 1U;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrSstSelect: // _GRSSTSELECT@4
        {
            const std::uint32_t board = context->glide_gate_stack[1];
            if (board == 0U)
            {
                context->glide_state.selected_board = 0U;
            }
            else
            {
                record_unsupported(
                    "board-selection-unsupported",
                    "only board zero is implemented");
            }
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrSstWinOpen: // _GRSSTWINOPEN@28
        {
            const std::uint32_t window = context->glide_gate_stack[1];
            const std::uint32_t resolution = context->glide_gate_stack[2];
            const std::uint32_t refresh = context->glide_gate_stack[3];
            const std::uint32_t color_format = context->glide_gate_stack[4];
            const std::uint32_t origin = context->glide_gate_stack[5];
            const std::uint32_t color_buffers = context->glide_gate_stack[6];
            const std::uint32_t auxiliary_buffers =
                context->glide_gate_stack[7];
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            const bool mode_supported = window == 0U && refresh == 0U &&
                repiu::hle::DecodeGlideResolution(
                    resolution, &width, &height);
            bool opened = false;
            try
            {
                opened = mode_supported &&
                    context->glide_backend.OpenWindowed(
                        width, height, color_buffers, auxiliary_buffers, origin);
            }
            catch (const std::exception& e)
            {
                fprintf(stderr, "[repiu-live-debug] _GRSSTWINOPEN@28 caught standard exception: %s\n", e.what());
            }
            catch (...)
            {
                fprintf(stderr, "[repiu-live-debug] _GRSSTWINOPEN@28 caught unknown exception\n");
            }
            context->glide_backend_message = context->glide_backend.message();
            if (!mode_supported)
            {
                record_unsupported(
                    "window-mode-unsupported",
                    "window, refresh, or resolution is unsupported");
            }
            else if (!opened)
            {
                record_backend_failure(
                    "window-open-backend-failure",
                    context->glide_backend_message);
            }
            fprintf(stderr, "[repiu-live-debug] _GRSSTWINOPEN@28: mode_supported=%d opened=%d message=%s\n",
                    mode_supported ? 1 : 0, opened ? 1 : 0, context->glide_backend_message.c_str());
            if (opened)
            {
                ++context->glide_window_open_count;
                if (context->shared_live_telemetry != nullptr)
                {
                    InterlockedExchange(
                        &context->shared_live_telemetry
                             ->glide_window_open_milestone,
                        1L);
                }
                context->glide_logical_width = width;
                context->glide_logical_height = height;
                context->glide_state.window_open = true;
                context->glide_state.width = width;
                context->glide_state.height = height;
                context->glide_state.color_format = color_format;
                context->glide_state.lfb_write_color_format = color_format;
                context->glide_state.origin = origin;
                context->glide_state.color_buffer_count = color_buffers;
                context->glide_state.auxiliary_buffer_count = auxiliary_buffers;
            }
            win32_context->Eax = opened ? 1U : 0U;
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 8U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrSstWinClose: // _GRSSTWINCLOSE@0
            context->glide_backend.Close();
            context->glide_state.window_open = false;
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += sizeof(std::uint32_t);
            return true;

        case go::kGrSstScreenWidth: // _GRSSTSCREENWIDTH@0
            win32_context->Eax = context->glide_state.width;
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += sizeof(std::uint32_t);
            return true;

        case go::kGrSstScreenHeight: // _GRSSTSCREENHEIGHT@0
            win32_context->Eax = context->glide_state.height;
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += sizeof(std::uint32_t);
            return true;

        case go::kGrTexTextureMemRequired: // _GRTEXTEXTUREMEMREQUIRED@8
        {
            repiu::hle::GlideTextureInfo info;
            const void* info_address = reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(context->glide_gate_stack[2]));
            std::uint32_t required_bytes = 0;
            if (!IsGuestRangeReadable(context, info_address, sizeof(info)))
            {
                return decline_gate("set-state-unreadable-memory");
            }
            // Task 332: the guest's GrTexInfo has to be READ. Until now the
            // pointer was only used for the readability check and a
            // default-constructed, all-zero struct went into the calculation,
            // so every texture answered 8192 bytes -- LOD 0 with aspect 8x1 at
            // one byte per texel. The guest sizes its own TMU address space from
            // this answer (see docs/kb/glide-texture-lod-and-formats.md), so it
            // was packing 256x256 maps 0x2000 apart and overwriting them.
            //
            // The guest layout is five 32-bit fields, confirmed from the raw
            // bytes at this pointer: smallLod, largeLod, aspectRatio, format,
            // data, matching `GlideTextureInfo` exactly.
            std::memcpy(&info, info_address, sizeof(info));
            if (!repiu::hle::CalculateGlideTextureMemoryRequired(
                    context->glide_gate_stack[1], info, &required_bytes))
            {
                return decline_gate("texture-memory-required-invalid-info");
            }
            // Task 332: the guest lays out its own TMU address space from this
            // answer, so the declared GrTexInfo and the bytes returned are the
            // other half of the size question the download arguments raise.
            {
                static const bool tex_census_enabled =
                    std::getenv("REPIU_GLIDE_TEX_CENSUS") != nullptr;
                static long mem_required_log_count = 0;
                if (tex_census_enabled &&
                    InterlockedIncrement(&mem_required_log_count) <= 24)
                {
                    // Every field decoded as zero while the pointer looks like a
                    // real guest address, so the raw bytes decide between "the
                    // guest really passes a zeroed struct" and "the struct is
                    // packed differently than this 5x32-bit reading assumes".
                    // Watcom sizes enums to the smallest type that fits, which
                    // would make GrTexInfo four bytes of enum plus a pointer.
                    char raw[3 * 24] = {};
                    const auto* raw_bytes =
                        reinterpret_cast<const std::uint8_t*>(info_address);
                    int written = 0;
                    if (IsGuestRangeReadable(context, info_address, 24U))
                    {
                        for (std::size_t i = 0; i < 24U; ++i)
                        {
                            written += std::snprintf(
                                raw + written,
                                sizeof(raw) - static_cast<std::size_t>(written),
                                "%02X ", raw_bytes[i]);
                        }
                    }
                    fprintf(stderr,
                            "[repiu-tex-args] memrequired evenOdd=%u ptr=0x%08X"
                            " smallLod=%u largeLod=%u aspect=%u format=%u"
                            " data=0x%08X -> bytes=%u raw=%s\n",
                            context->glide_gate_stack[1],
                            context->glide_gate_stack[2], info.small_lod,
                            info.large_lod, info.aspect_ratio, info.format,
                            info.data, required_bytes, raw);
                }
            }
            ++context->glide_gate_handled_count;
            win32_context->Eax = required_bytes;
            win32_context->Eip = return_address;
            win32_context->Esp += 3U * sizeof(std::uint32_t);
            return true;
        }
        case go::kGrTexDownloadTable: // _GRTEXDOWNLOADTABLE@12
        {
            // grTexDownloadTable(GrChipID_t tmu, GrTexTable_t type, void* data).
            // The frame includes the return address before its three arguments.
            std::uint32_t frame[4] = {};
            const auto* guest_stack = reinterpret_cast<const std::uint32_t*>(
                static_cast<std::uintptr_t>(win32_context->Esp));
            GlideTexDownloadTableCall call;
            call.stack_advance = sizeof(frame);
            if (IsGuestRangeReadable(context, guest_stack, sizeof(frame)))
            {
                std::memcpy(frame, guest_stack, sizeof(frame));
                DecodeGlideTexDownloadTableCall(
                    frame, sizeof(frame) / sizeof(frame[0]), &call);
                const auto* data = reinterpret_cast<const std::uint32_t*>(
                    static_cast<std::uintptr_t>(call.data));
                if (call.type != 2U)
                {
                    record_unsupported(
                        "texture-table-type-unsupported",
                        "only GR_TEXTABLE_PALETTE is implemented");
                }
                else if (!IsGuestRangeReadable(context, data, 1024U))
                {
                    record_backend_failure(
                        "texture-table-unreadable-memory",
                        "palette source is not guest-readable");
                }
                else
                {
                    // Standard Glide palettes carry 8-bit RGB in the low 24
                    // bits. The high byte is ignored; AP_88 supplies alpha in
                    // the texel itself and P_8 remains opaque.
                    context->glide_state.palette_valid =
                        repiu::hle::DecodeGlidePaletteToRgba8(
                            data, 256U,
                            context->glide_state.palette_rgba8.data(),
                            context->glide_state.palette_rgba8.size());
                    if (context->glide_state.palette_valid &&
                        !context->glide_backend.RefreshPalettizedTextures(
                            context->glide_state.palette_rgba8.data()))
                    {
                        record_backend_failure(
                            "texture-palette-refresh-failed",
                            "stored palettized textures could not be refreshed");
                    }
                }
            }
            else
            {
                record_backend_failure(
                    "texture-table-unreadable-stack",
                    "arguments are not guest-readable");
            }
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += call.stack_advance;
            return true;
        }

        case go::kGrTexDownloadMipMapLevel: // _GRTEXDOWNLOADMIPMAPLEVEL@32
        {
            // R3: decode and upload the texel image so a later grTexSource can bind
            // it. Args (from the guest stack, the mirror holds only 8 dwords):
            // tmu, startAddress, thisLod, largeLod, aspectRatio, format, evenOdd,
            // data. Preserve the stdcall ABI regardless of upload success.
            std::uint32_t args[9] = {};
            const auto* guest_stack = reinterpret_cast<const std::uint32_t*>(
                static_cast<std::uintptr_t>(win32_context->Esp));
            if (IsGuestRangeReadable(context, guest_stack, sizeof(args)))
            {
                std::memcpy(args, guest_stack, sizeof(args));
                const std::uint32_t start_address = args[2];
                const std::uint32_t large_lod = args[4];
                const std::uint32_t aspect_ratio = args[5];
                const std::uint32_t format = args[6];
                const auto* data = reinterpret_cast<const std::uint8_t*>(
                    static_cast<std::uintptr_t>(args[8]));
                repiu::hle::GlideTextureDimensions dimensions;
                const bool dimensions_ok =
                    repiu::hle::CalculateGlideTextureDimensions(
                        large_lod, aspect_ratio, &dimensions);
                static const bool format_census_enabled =
                    std::getenv("REPIU_GLIDE_TEX_CENSUS") != nullptr;
                // Format census (env-gated): count every download per format and
                // record why one was dropped. Palette formats (P_8, AP_88) use the
                // last downloaded palette when available, and the NCC formats are
                // refused outright -- this separates "the
                // game never uses that format" from "we silently drop it".
                // Task 332: the raw arguments, because two readings of this run
                // conflict. The font atlas decodes correctly as 256x256, yet the
                // guest packs its textures 0x2000 apart, which is exactly one
                // 64x64 16-bit map. Only the values the game actually passes can
                // settle which size it means, so log every argument rather than
                // the derived dimensions.
                if (format_census_enabled)
                {
                    // Logging only the opening downloads missed the sprites
                    // under investigation, which arrive with the select screen.
                    // There are well under two hundred in a run.
                    static long download_log_count = 0;
                    if (InterlockedIncrement(&download_log_count) <= 200)
                    {
                        fprintf(stderr,
                                "[repiu-tex-args] download tmu=%u addr=0x%08X"
                                " thisLod=%u largeLod=%u aspect=%u format=%u"
                                " evenOdd=%u data=0x%08X -> dims=%s%ux%u\n",
                                args[1], start_address, args[3], large_lod,
                                aspect_ratio, format, args[7], args[8],
                                dimensions_ok ? "" : "INVALID ",
                                dimensions_ok ? dimensions.width : 0U,
                                dimensions_ok ? dimensions.height : 0U);
                    }
                }
                if (format_census_enabled && format < 16U)
                {
                    static long format_counts[16] = {};
                    const long seen = InterlockedIncrement(&format_counts[format]);
                    if (seen <= 3)
                    {
                        fprintf(stderr,
                                "[repiu-tex-census] format=%u lod=%u aspect=%u"
                                " dims=%s%ux%u acceptable=%d addr=0x%08X seen=%ld\n",
                                format, large_lod, aspect_ratio,
                                dimensions_ok ? "" : "INVALID ",
                                dimensions_ok ? dimensions.width : 0U,
                                dimensions_ok ? dimensions.height : 0U,
                                repiu::hle::IsGlideTextureFormatAcceptable(format)
                                    ? 1 : 0,
                                start_address, seen);
                    }
                }
                if (dimensions_ok)
                {
                    const std::size_t bytes_per_texel = format >= 8U ? 2U : 1U;
                    const std::size_t source_size =
                        static_cast<std::size_t>(dimensions.width) *
                        dimensions.height * bytes_per_texel;
                    if (source_size == 0U)
                    {
                        record_unsupported(
                            "texture-download-empty-source",
                            "calculated source size is zero");
                    }
                    else if (!IsGuestRangeReadable(
                                 context, data, source_size))
                    {
                        record_backend_failure(
                            "texture-download-unreadable-memory",
                            "texture source is not guest-readable");
                    }
                    else
                    {
                        const std::uint8_t* palette_ptr =
                            context->glide_state.palette_valid
                            ? context->glide_state.palette_rgba8.data()
                            : nullptr;
                        const bool stored = context->glide_backend.StoreTexture(
                            start_address, format, large_lod, aspect_ratio, data,
                            source_size, palette_ptr);
                        context->glide_backend_message =
                            context->glide_backend.message();
                        if (!stored)
                        {
                            if (std::string_view(
                                    context->glide_backend_message)
                                    .starts_with("unsupported Glide"))
                            {
                                record_unsupported(
                                    "texture-download-unsupported",
                                    context->glide_backend_message);
                            }
                            else
                            {
                                record_backend_failure(
                                    "texture-download-backend-failure",
                                    context->glide_backend_message);
                            }
                        }
                        if (format_census_enabled && !stored)
                        {
                            static long store_fail_log = 0;
                            if (InterlockedIncrement(&store_fail_log) <= 12)
                            {
                                fprintf(stderr,
                                        "[repiu-tex-census] STORE FAILED format=%u"
                                        " %ux%u addr=0x%08X reason=%s\n",
                                        format, dimensions.width, dimensions.height,
                                        start_address,
                                        context->glide_backend_message.c_str());
                            }
                        }
                    }
                }
                else
                {
                    record_unsupported(
                        "texture-dimensions-unsupported",
                        "LOD or aspect ratio is unsupported");
                }
            }
            else
            {
                record_backend_failure(
                    "texture-download-unreadable-stack",
                    "arguments are not guest-readable");
            }
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 9U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrTexSource: // _GRTEXSOURCE@16
            // R3: select the current texture (args: tmu, startAddress, evenOdd,
            // GrTexInfo*). A missing texture simply leaves the previous binding.
            if (!context->glide_backend.SourceTexture(
                    context->glide_gate_stack[2]))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                if (std::string_view(context->glide_backend_message)
                        .starts_with("unsupported Glide"))
                {
                    record_unsupported(
                        "texture-source-unsupported",
                        context->glide_backend_message);
                }
                else
                {
                    record_backend_failure(
                        "texture-source-backend-failure",
                        context->glide_backend_message);
                }
            }
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += sizeof(std::uint32_t) +
                glide_export->argument_byte_count;
            return true;

        case go::kGrTexMinAddress: // _GRTEXMINADDRESS@4
            // stdcall: pop return address and the one dword argument. fxTMInit is
            // the sole caller and issues `push arg; call grTexMin; push arg; call
            // grTexMax; mov eax,[esp]` with no caller-side cleanup, so the gate
            // must clean the argument or the reload reads a leftover (NULL gc).
            if (context->glide_gate_stack[1] == 0U)
            {
                RecordGlideTextureGateTrace(context, win32_context, *glide_export, return_address, 0U, false);
                ++context->glide_gate_handled_count;
                win32_context->Eax = 0U;
                win32_context->Eip = return_address;
                win32_context->Esp += 2U * sizeof(std::uint32_t);
                return true;
            }
            break;

        case go::kGrTexMaxAddress: // _GRTEXMAXADDRESS@4
            // stdcall: pop return address and the one dword argument (see the
            // grTexMinAddress note above; fxTMInit relies on callee cleanup).
            if (context->glide_gate_stack[1] == 0U)
            {
                std::uint32_t maximum_address = 0;
                if (!repiu::hle::CalculateGlideTextureMaxAddress(
                        context->glide_state.texture_memory_bytes,
                        &maximum_address))
                {
                    return decline_gate("texture-max-address-calculation-failure");
                }
                RecordGlideTextureGateTrace(context, win32_context, *glide_export, return_address, maximum_address, true);
                ++context->glide_gate_handled_count;
                win32_context->Eax = maximum_address;
                win32_context->Eip = return_address;
                win32_context->Esp += 2U * sizeof(std::uint32_t);
                return true;
            }
            break;

        case go::kGrColorMask: // _GRCOLORMASK@8
        {
            const bool rgb = context->glide_gate_stack[1] != 0U;
            const bool alpha = context->glide_gate_stack[2] != 0U;
            if (!context->glide_backend.SetColorMask(rgb, alpha))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                return decline_gate("color-mask-backend-failure");
            }
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 3U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrRenderBuffer: // _GRRENDERBUFFER@4
            if (!context->glide_backend.SetRenderBuffer(
                    context->glide_gate_stack[1]))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                return decline_gate("render-buffer-backend-failure");
            }
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;

        case go::kGrDepthBiasLevel: // _GRDEPTHBIASLEVEL@4
            RecordGlideImplementationIssue(
                win32_context,
                context,
                *glide_export,
                repiu::hle::GlideImplementationIssueKind::
                    kUnimplementedFunction,
                "depth-bias-noop",
                "depth bias accepted without backend state",
                "continue");
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;

        case go::kGrDepthMask: // _GRDEPTHMASK@4
            if (!context->glide_backend.SetDepthMask(
                    context->glide_gate_stack[1] != 0U))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                return decline_gate("depth-mask-backend-failure");
            }
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;

        case go::kGrDepthBufferMode: // _GRDEPTHBUFFERMODE@4
            if (!context->glide_backend.SetDepthBufferMode(
                    context->glide_gate_stack[1]))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                return decline_gate("depth-buffer-mode-backend-failure");
            }
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;

        case go::kGrLfbWriteColorFormat: // _GRLFBWRITECOLORFORMAT@4
            context->glide_state.lfb_write_color_format =
                context->glide_gate_stack[1];
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;

        case go::kGrAlphaCombine: // _GRALPHACOMBINE@20
        {
            // Same retain policy as color combine (design 237, Task 247):
            // an equation the GLSL translator merely does not support yet
            // must still preserve the stdcall ABI, or the unhandled gate
            // leaks its 24-byte frame and the caller epilogue returns to 0
            // (the Task 245/246 zero-EIP root cause).
            repiu::hle::GlideAlphaCombineState state;
            state.function = context->glide_gate_stack[1];
            state.factor = context->glide_gate_stack[2];
            state.local = context->glide_gate_stack[3];
            state.other = context->glide_gate_stack[4];
            state.invert = context->glide_gate_stack[5] != 0U;
            state.valid = true;
            if (!context->glide_backend.SetAlphaCombine(state))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                if (context->glide_backend_message !=
                    "unsupported Glide alpha-combine equation")
                {
                    return decline_gate("alpha-combine-backend-failure");
                }
                RecordGlideImplementationIssue(
                    win32_context,
                    context,
                    *glide_export,
                    repiu::hle::GlideImplementationIssueKind::
                        kUnsupportedArgument,
                    "alpha-combine-unsupported",
                    context->glide_backend_message,
                    "continue");
            }
            context->glide_state.alpha_combine = state;
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 6U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrColorCombine: // _GRCOLORCOMBINE@20
        {
            repiu::hle::GlideColorCombineState state;
            state.function = context->glide_gate_stack[1];
            state.factor = context->glide_gate_stack[2];
            state.local = context->glide_gate_stack[3];
            state.other = context->glide_gate_stack[4];
            state.invert = context->glide_gate_stack[5] != 0U;
            state.valid = true;
            constexpr std::uint32_t kCombineFunctionScaleOther = 3U;
            constexpr std::uint32_t kCombineOtherTexture = 1U;
            // R3: function 3 (GR_COMBINE_FUNCTION_SCALE_OTHER) with other=1
            // (GR_COMBINE_OTHER_TEXTURE) routes the fragment output to the sourced
            // texture; function 1 (LOCAL) is the iterated vertex color. Toggle the
            // backend texture-combine path accordingly (observed content draws use
            // function 3, init uses function 1).
            context->glide_backend.SetTextureCombineEnabled(
                state.function == kCombineFunctionScaleOther &&
                state.other == kCombineOtherTexture);
            if (!context->glide_backend.SetColorCombine(state))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                if (context->glide_backend_message !=
                    "unsupported Glide color-combine equation")
                {
                    return decline_gate("color-combine-backend-failure");
                }
                RecordGlideImplementationIssue(
                    win32_context,
                    context,
                    *glide_export,
                    repiu::hle::GlideImplementationIssueKind::
                        kUnsupportedArgument,
                    "color-combine-unsupported",
                    context->glide_backend_message,
                    "continue");
            }
            context->glide_state.color_combine = state;
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 6U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrAlphaBlendFunction: // _GRALPHABLENDFUNCTION@16
        {
            // Same retain policy as the combine gates (design 237): a blend
            // function the backend does not express yet must still preserve
            // the stdcall ABI, or the unhandled gate leaks its frame (the
            // Task 246 corruption chain).
            repiu::hle::GlideAlphaBlendState state;
            state.rgb_source = context->glide_gate_stack[1];
            state.rgb_destination = context->glide_gate_stack[2];
            state.alpha_source = context->glide_gate_stack[3];
            state.alpha_destination = context->glide_gate_stack[4];
            state.valid = true;
            if (!context->glide_backend.SetAlphaBlend(state))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                if (context->glide_backend_message !=
                    "unsupported Glide alpha-blend function")
                {
                    return decline_gate("alpha-blend-backend-failure");
                }
                RecordGlideImplementationIssue(
                    win32_context,
                    context,
                    *glide_export,
                    repiu::hle::GlideImplementationIssueKind::
                        kUnsupportedArgument,
                    "alpha-blend-unsupported",
                    context->glide_backend_message,
                    "continue");
            }
            context->glide_state.alpha_blend = state;
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 5U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrAlphaTestFunction: // _GRALPHATESTFUNCTION@4
        {
            const std::uint32_t function = context->glide_gate_stack[1];
            if (!context->glide_backend.SetAlphaTestFunction(function))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                return decline_gate("alpha-test-function-backend-failure");
            }
            context->glide_state.alpha_test_function = function;
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrAlphaTestReferenceValue: // _GRALPHATESTREFERENCEVALUE@4
        {
            const std::uint32_t reference_value = context->glide_gate_stack[1];
            if (!context->glide_backend.SetAlphaTestReferenceValue(reference_value))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                return decline_gate("alpha-test-reference-value-backend-failure");
            }
            context->glide_state.alpha_test_reference = reference_value;
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrDepthBufferFunction: // _GRDEPTHBUFFERFUNCTION@4
        {
            const std::uint32_t function = context->glide_gate_stack[1];
            if (!context->glide_backend.SetDepthBufferFunction(function))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                return decline_gate("depth-buffer-function-backend-failure");
            }
            context->glide_state.depth_buffer_function = function;
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrClipWindow: // _GRCLIPWINDOW@16
        {
            const std::uint32_t min_x = context->glide_gate_stack[1];
            const std::uint32_t min_y = context->glide_gate_stack[2];
            const std::uint32_t max_x = context->glide_gate_stack[3];
            const std::uint32_t max_y = context->glide_gate_stack[4];
            if (!context->glide_backend.SetClipWindow(
                    min_x, min_y, max_x, max_y))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                return decline_gate("clip-window-backend-failure");
            }
            context->glide_state.clip_min_x = min_x;
            context->glide_state.clip_min_y = min_y;
            context->glide_state.clip_max_x = max_x;
            context->glide_state.clip_max_y = max_y;
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 5U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrCullMode: // _GRCULLMODE@4
        {
            const std::uint32_t mode = context->glide_gate_stack[1];
            if (!context->glide_backend.SetCullMode(mode))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                return decline_gate("cull-mode-backend-failure");
            }
            context->glide_state.cull_mode = mode;
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrGlideGetState: // _GRGLIDEGETSTATE@4
        {
            repiu::hle::GlideStateImage image;
            void* output = reinterpret_cast<void*>(static_cast<std::uintptr_t>(
                context->glide_gate_stack[1]));
            if (!repiu::hle::BuildGlideStateImage(context->glide_state, &image) ||
                !WriteGuestBytes(context, output, image.data(), image.size()))
            {
                return decline_gate("get-state-serialization-failure");
            }
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrGlideSetState: // _GRGLIDESETSTATE@4
        {
            repiu::hle::GlideStateImage image;
            const void* input = reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(context->glide_gate_stack[1]));
            repiu::hle::GlideLogicalState restored = context->glide_state;
            if (!IsGuestRangeReadable(context, input, image.size()))
            {
                return decline_gate("set-state-unreadable-memory");
            }
            std::memcpy(image.data(), input, image.size());
            if (!repiu::hle::ParseGlideStateImage(image, &restored))
            {
                return decline_gate("set-state-deserialization-failure");
            }
            context->glide_state = restored;
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrDitherMode: // _GRDITHERMODE@4
        {
            const std::uint32_t mode = context->glide_gate_stack[1];
            if (!context->glide_backend.SetDitherMode(mode))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                return decline_gate("dither-mode-backend-failure");
            }
            context->glide_state.dither_mode = mode;
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrBufferClear: // _GRBUFFERCLEAR@12
        {
            // Task 332: same GrColor_t conversion as grConstantColorValue.
            const std::uint32_t color = repiu::hle::ConvertGlideColorToArgb(
                context->glide_gate_stack[1],
                context->glide_state.color_format);
            const std::uint32_t alpha = context->glide_gate_stack[2];
            const std::uint32_t depth = context->glide_gate_stack[3];
            if (GlideAsyncPresentEnabled())
            {
                // Posted so the wait the swap no longer takes does not simply
                // reappear here, one gate later.
                context->glide_backend.PostBufferClear(color, alpha, depth);
            }
            else if (!context->glide_backend.BufferClear(color, alpha, depth))
            {
                context->glide_backend_message = context->glide_backend.message();
                return decline_gate("buffer-clear-backend-failure");
            }
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 4U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrBufferSwap: // _GRBUFFERSWAP@4
        {
            // Task 332: every filter tried so far (quad size, texture size)
            // spent its sample budget on other geometry, so dump whole frames
            // instead. One complete frame of the screen in question lists the
            // dot draws next to everything else and needs no guess about what
            // distinguishes them.
            if (Win32GlideFrameDumpEnabled())
            {
                Win32GlideAdvanceFrameDump();
            }
            const std::uint32_t swap_interval = context->glide_gate_stack[1];
            if (GlideAsyncPresentEnabled())
            {
                // The whole point of the task: the guest returns here instead of
                // waiting out the vblank, which with vsync on is 10.8 ms and
                // 32.8% of guest-run. A failure afterwards is counted rather
                // than declining a gate that has already returned.
                context->glide_backend.PostBufferSwap(swap_interval);
            }
            else if (!context->glide_backend.BufferSwap(swap_interval))
            {
                context->glide_backend_message = context->glide_backend.message();
                return decline_gate("buffer-swap-backend-failure");
            }
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrBufferNumPending: // _GRBUFFERNUMPENDING@0
            // Task 440: the game calls this exactly once per swap -- it is
            // Glide's throttle, and answering a constant zero is what made it
            // inert. With the present posted, the real outstanding count is what
            // lets the game pace itself the way the hardware let it.
            ++context->glide_gate_handled_count;
            win32_context->Eax =
                context->glide_backend.glide_pending_swap_count();
            win32_context->Eip = return_address;
            win32_context->Esp += sizeof(std::uint32_t);
            return true;

        // Task 420: the AA variants carry the same geometry and differ only by
        // antialiasing flags this backend does not implement, so they render the
        // same vertices rather than nothing. `_GRAADRAWLINE@8` takes the same two
        // pointers as `_GRDRAWLINE@8`, so the stack adjustment is shared too.
        case go::kGrAADrawLine: // _GRAADRAWLINE@8
        case go::kGrDrawLine: // _GRDRAWLINE@8
        {
            // `kGrDrawLine` has always drawn, so only the antialiased variant
            // reverts under the switch.
            if (!draw_entry_points_enabled &&
                glide_export->gate_id == go::kGrAADrawLine)
            {
                record_unimplemented("catalog-default-handler",
                                     "ABI-preserving default return");
                ++context->glide_gate_handled_count;
                win32_context->Eip = return_address;
                win32_context->Esp += 3U * sizeof(std::uint32_t);
                return true;
            }
            hle::GlideDrawVertex vertices[2] = {};
            for (std::size_t index = 0U; index < 2U; ++index)
            {
                const auto* source = reinterpret_cast<const std::uint32_t*>(
                    static_cast<std::uintptr_t>(
                        context->glide_gate_stack[index + 1U]));
                if (!IsGuestRangeReadable(context, source,
                                          hle::kGlideProducerVertexByteCount))
                {
                    return decline_gate("draw-line-unreadable-vertex");
                }
                std::uint32_t producer[
                    hle::kGlideProducerVertexDwordCount] = {};
                std::memcpy(producer, source, sizeof(producer));
                if (!hle::DecodeGlideProducerVertex(
                        producer, hle::kGlideProducerVertexDwordCount,
                        &vertices[index]))
                {
                    return decline_gate("draw-line-decode-failure");
                }
            }
            if (!QueueGlideDrawForBatch(context, draw_batch, vertices, 2U,
                                        Win32GlideBatchPrimitive::kLines) &&
                !context->glide_backend.DrawLine(vertices[0], vertices[1]))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                return decline_gate("draw-line-backend-failure");
            }
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 3U * sizeof(std::uint32_t);
            return true;
        }

        // Task 420: three vertex pointers followed by three antialiasing flags
        // this backend does not implement, so the flags are ignored and the
        // geometry is drawn. Seven stack slots, unlike `_GRDRAWTRIANGLE@12`.
        case go::kGrAADrawTriangle: // _GRAADRAWTRIANGLE@24
        {
            if (!draw_entry_points_enabled)
            {
                record_unimplemented("catalog-default-handler",
                                     "ABI-preserving default return");
                ++context->glide_gate_handled_count;
                win32_context->Eip = return_address;
                win32_context->Esp += 7U * sizeof(std::uint32_t);
                return true;
            }
            hle::GlideDrawVertex vertices[3] = {};
            for (std::size_t index = 0U; index < 3U; ++index)
            {
                const auto* source = reinterpret_cast<const std::uint32_t*>(
                    static_cast<std::uintptr_t>(
                        context->glide_gate_stack[index + 1U]));
                if (!IsGuestRangeReadable(context, source,
                                          hle::kGlideProducerVertexByteCount))
                {
                    return decline_gate("aa-draw-triangle-unreadable-vertex");
                }
                std::uint32_t producer[
                    hle::kGlideProducerVertexDwordCount] = {};
                std::memcpy(producer, source, sizeof(producer));
                if (!hle::DecodeGlideProducerVertex(
                        producer, hle::kGlideProducerVertexDwordCount,
                        &vertices[index]))
                {
                    return decline_gate("aa-draw-triangle-decode-failure");
                }
            }
            if (!QueueGlideDrawForBatch(context, draw_batch, vertices, 3U,
                                        Win32GlideBatchPrimitive::kTriangles) &&
                !context->glide_backend.DrawTriangle(vertices[0], vertices[1],
                                                     vertices[2]))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                return decline_gate("aa-draw-triangle-backend-failure");
            }
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 7U * sizeof(std::uint32_t);
            return true;
        }

        // Task 420: this used to accept the request and discard it, so anything
        // a title drew as a point was missing from the screen with nothing to
        // show for it. pumpit2 calls it (Task 420 design section 1).
        case go::kGrAADrawPoint: // _GRAADRAWPOINT@4
        case go::kGrDrawPoint: // _GRDRAWPOINT@4
        {
            if (!draw_entry_points_enabled)
            {
                record_unimplemented(
                    glide_export->gate_id == go::kGrDrawPoint
                        ? "draw-point-noop"
                        : "catalog-default-handler",
                    "draw request accepted without rendering");
                ++context->glide_gate_handled_count;
                win32_context->Eip = return_address;
                win32_context->Esp += 2U * sizeof(std::uint32_t);
                return true;
            }
            const auto* source = reinterpret_cast<const std::uint32_t*>(
                static_cast<std::uintptr_t>(context->glide_gate_stack[1]));
            if (!IsGuestRangeReadable(context, source,
                                      hle::kGlideProducerVertexByteCount))
            {
                return decline_gate("draw-point-unreadable-vertex");
            }
            std::uint32_t producer[hle::kGlideProducerVertexDwordCount] = {};
            std::memcpy(producer, source, sizeof(producer));
            hle::GlideDrawVertex vertex = {};
            if (!hle::DecodeGlideProducerVertex(
                    producer, hle::kGlideProducerVertexDwordCount, &vertex))
            {
                return decline_gate("draw-point-decode-failure");
            }
            if (!QueueGlideDrawForBatch(context, draw_batch, &vertex, 1U,
                                        Win32GlideBatchPrimitive::kPoints) &&
                !context->glide_backend.DrawPoint(vertex))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                return decline_gate("draw-point-backend-failure");
            }
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGuFogGenerateExp: // _GUFOGGENERATEEXP@8
        {
            const std::uint32_t table_ptr = context->glide_gate_stack[1];
            float density = 0.0f;
            std::memcpy(&density, &context->glide_gate_stack[2], sizeof(float));

            if (table_ptr != 0U && IsGuestRangeWritable(context, reinterpret_cast<void*>(static_cast<std::uintptr_t>(table_ptr)), 64U))
            {
                std::uint8_t* table = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(table_ptr));
                for (std::size_t i = 0; i < 64U; ++i)
                {
                    float factor = 1.0f - std::exp(-density * (static_cast<float>(i) * 4.0f / 255.0f));
                    if (factor < 0.0f) factor = 0.0f;
                    if (factor > 1.0f) factor = 1.0f;
                    table[i] = static_cast<std::uint8_t>(factor * 255.0f);
                }
            }
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 3U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrFogMode: // _GRFOGMODE@4
        {
            const std::uint32_t mode = context->glide_gate_stack[1];
            if (!context->glide_backend.SetFogMode(mode))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                return decline_gate("fog-mode-backend-failure");
            }
            context->glide_state.fog_mode = mode;
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrFogColorValue: // _GRFOGCOLORVALUE@4
            context->glide_state.fog_color =
                repiu::hle::ConvertGlideColorToArgb(
                    context->glide_gate_stack[1],
                    context->glide_state.color_format);
            if (!context->glide_backend.SetFogColor(
                    context->glide_state.fog_color))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                return decline_gate("fog-color-backend-failure");
            }
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;

        case go::kGrFogTable: // _GRFOGTABLE@4
        {
            context->glide_state.fog_table_pointer =
                context->glide_gate_stack[1];
            const auto* table = reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(
                    context->glide_state.fog_table_pointer));
            if (!IsGuestRangeReadable(
                    context, table,
                    repiu::hle::kGlideFogTableEntryCount))
            {
                return decline_gate("fog-table-unreadable-memory");
            }
            std::memcpy(context->glide_state.fog_table.data(), table,
                        context->glide_state.fog_table.size());
            context->glide_state.fog_table_valid = true;
            if (!context->glide_backend.SetFogTable(
                    context->glide_state.fog_table))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                return decline_gate("fog-table-backend-failure");
            }
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrConstantColorValue: // _GRCONSTANTCOLORVALUE@4
            // R4: retain the constant color so a later CONSTANT combine source can
            // read it. Observed value during the content phase is 0xFFFFFFFF.
            //
            // Task 332: a GrColor_t is laid out per the GrColorFormat_t chosen at
            // grSstWinOpen, and PIU selects ABGR, so the value has to be
            // converted. Most of the game's constants are greys, which are
            // symmetric and hid this; the difficulty dots' 0xFE6565FE is red in
            // ABGR and came out blue when read as ARGB.
            context->glide_state.constant_color =
                repiu::hle::ConvertGlideColorToArgb(
                    context->glide_gate_stack[1],
                    context->glide_state.color_format);
            if (!context->glide_backend.SetConstantColor(context->glide_state.constant_color))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                return decline_gate("set-state-backend-failure");
            }
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;

        case go::kGrTexClampMode: // _GRTEXCLAMPMODE@12
        {
            // Texture sampler parameters stay within the rendering boundary; the
            // observed filter/clamp/mipmap modes are handled by the backend texture
            // defaults for now.
            const std::uint32_t s_clamp = context->glide_gate_stack[2];
            const std::uint32_t t_clamp = context->glide_gate_stack[3];
            context->glide_backend.SetTextureClampMode(s_clamp, t_clamp);
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 4U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrTexFilterMode: // _GRTEXFILTERMODE@12
        {
            const std::uint32_t min_filter = context->glide_gate_stack[2];
            const std::uint32_t mag_filter = context->glide_gate_stack[3];
            context->glide_backend.SetTextureFilterMode(min_filter, mag_filter);
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 4U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrTexMipMapMode: // _GRTEXMIPMAPMODE@12
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 4U * sizeof(std::uint32_t);
            return true;

        case go::kGrTexCombine: // _GRTEXCOMBINE@28
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 8U * sizeof(std::uint32_t);
            return true;
        case go::kGrDrawTriangle: // _GRDRAWTRIANGLE@12
        {
            // Decode the confirmed 60-byte 2-TMU GrVertex fields: x/y (dwords 0/1),
            // iterated color r/g/b/a in [0..255] (dwords 3/4/5/7), vertex oow
            // (dword 8), and TMU0 sow/tow (dwords 9/10). Dwords 11..14 are
            // variable in the captured 60-byte producer layout, so the
            // observed non-projected texture path shares dword 8's oow for
            // both perspective correction and table fog.
            Win32GlideTriangleObservation& triangle =
                context->glide_first_triangle;
            if (!triangle.valid)
            {
                triangle.valid = true;
                for (std::size_t index = 0; index < 3U; ++index)
                {
                    triangle.pointers[index] = context->glide_gate_stack[index + 1U];
                    const auto* vertex = reinterpret_cast<const void*>(
                        static_cast<std::uintptr_t>(triangle.pointers[index]));
                    triangle.pointer_readable[index] = IsGuestRangeReadable(
                        context, vertex, sizeof(triangle.dwords[index]));
                    if (triangle.pointer_readable[index])
                    {
                        std::memcpy(triangle.dwords[index], vertex,
                                    sizeof(triangle.dwords[index]));
                    }
                }
                fprintf(stderr,
                        "[repiu-live] Glide first triangle vertices: %08X/%d %08X/%d %08X/%d\n",
                        triangle.pointers[0], triangle.pointer_readable[0],
                        triangle.pointers[1], triangle.pointer_readable[1],
                        triangle.pointers[2], triangle.pointer_readable[2]);
                for (std::size_t index = 0; index < 3U; ++index)
                {
                    fprintf(stderr, "[repiu-live] Glide first triangle vertex %u dwords:",
                            static_cast<unsigned>(index));
                    for (std::uint32_t dword : triangle.dwords[index])
                    {
                        fprintf(stderr, " %08X", dword);
                    }
                    fprintf(stderr, "\n");
                }
            }
            const std::uint32_t sequence = ++context->glide_triangle_trace_count;
            Win32GlideTriangleTraceEntry& trace = context->glide_triangle_trace[
                (sequence - 1U) % kWin32GlideTriangleTraceCapacity];
            trace = Win32GlideTriangleTraceEntry{};
            trace.valid = true;
            trace.sequence = sequence;
            for (std::size_t index = 0; index < 3U; ++index)
            {
                trace.pointers[index] = context->glide_gate_stack[index + 1U];
                const auto* vertex = reinterpret_cast<const void*>(
                    static_cast<std::uintptr_t>(trace.pointers[index]));
                trace.pointer_readable[index] = IsGuestRangeReadable(
                    context, vertex, sizeof(trace.dwords[index]));
                if (trace.pointer_readable[index])
                {
                    std::memcpy(trace.dwords[index], vertex,
                                sizeof(trace.dwords[index]));
                }
            }
            if (sequence > kWin32GlideTriangleTraceCapacity)
            {
                context->glide_triangle_trace_wrapped = true;
            }
            hle::GlideDrawVertex vertices[3] = {};
            for (std::size_t index = 0; index < 3U; ++index)
            {
                if (!trace.pointer_readable[index])
                {
                    return decline_gate("compact-triangle-unreadable-vertex");
                }
                if (!hle::DecodeGlideProducerVertex(
                        trace.dwords[index],
                        kWin32GlideProducerVertexDwordCount,
                        &vertices[index]))
                {
                    return decline_gate("compact-triangle-decode-failure");
                }
            }
            static const bool draw_diagnostic_enabled =
                std::getenv("REPIU_GLIDE_DRAW_DIAG") != nullptr;
            const float min_x = std::min({vertices[0].x, vertices[1].x, vertices[2].x});
            const float max_x = std::max({vertices[0].x, vertices[1].x, vertices[2].x});
            const float min_y = std::min({vertices[0].y, vertices[1].y, vertices[2].y});
            const float max_y = std::max({vertices[0].y, vertices[1].y, vertices[2].y});
            const bool is_large = (max_x - min_x) >= 100.0F && (max_y - min_y) >= 100.0F;
            static long large_diag_count = 0;
            // Draw diagnostic (env-gated): the swap-driven pixel sampler only fires
            // on selected swap indices, so it can miss the content phase entirely.
            // Log the decoded vertices and read the back buffer straight after the
            // draw, which separates "geometry is degenerate", "state kills the
            // fragment", and "it draws but something later erases it".
            // A missing full-screen background would never appear in a first-N
            // sample if the game submits it after the UI text, so also diagnose any
            // triangle large enough to be background geometry regardless of when it
            // arrives.
            const bool diagnose_large =
                is_large && InterlockedIncrement(&large_diag_count) <= 12;
            std::size_t before_non_black = 0;
            const bool diagnose_this_draw =
                draw_diagnostic_enabled && (sequence <= 12U || diagnose_large);
            if (diagnose_this_draw)
            {
                std::vector<std::uint8_t> before;
                if (context->glide_backend.ReadbackFramebuffer(
                        context->glide_state.width, context->glide_state.height,
                        &before))
                {
                    for (std::size_t i = 0; i + 3U < before.size(); i += 4U)
                    {
                        if (before[i] > 8U || before[i + 1U] > 8U ||
                            before[i + 2U] > 8U)
                        {
                            ++before_non_black;
                        }
                    }
                }
            }
            // Task 332 draw census (env-gated): the reported symptom is that the
            // small difficulty-level dots are missing while everything around
            // them draws. Three causes produce that and need different fixes --
            // the game never submits those quads, it submits them with a
            // texture address that was never downloaded (so the draw is
            // untextured), or it submits them textured and something later
            // covers them. Bucketing every draw by bounding-box size and
            // recording what each had bound separates the three. Small quads
            // are sampled individually because they are the ones in question.
            {
                static const bool draw_census_enabled =
                    std::getenv("REPIU_GLIDE_DRAW_CENSUS") != nullptr;
                if (draw_census_enabled || Win32GlideFrameDumpEnabled())
                {
                    const float width = max_x - min_x;
                    const float height = max_y - min_y;
                    // `small` is a windows.h macro, so the flag is named
                    // explicitly.
                    const bool is_small_quad =
                        width <= 48.0F && height <= 48.0F;
                    const bool textured =
                        context->glide_backend.has_current_texture();
                    static long small_draws = 0;
                    static long small_untextured = 0;
                    static long total_draws = 0;
                    static long small_samples = 0;
                    ++total_draws;
                    // A texture smaller than the 256-texel Glide coordinate
                    // space is where the two readings of the coordinate
                    // convention diverge, and it is what the dots and the
                    // arrows use, so those draws are sampled whatever their
                    // size.
                    const bool small_texture = textured &&
                        (context->glide_backend.current_texture_width() <
                             256U ||
                         context->glide_backend.current_texture_height() <
                             256U);
                    // The symptom itself: the dots reach the screen as a few
                    // pixels where the original draws roughly fourteen. Nothing
                    // else on this screen is that small, so filtering on it
                    // samples exactly the draws in question instead of spending
                    // the budget on text and fading panels.
                    if (g_frame_dump_active)
                    {
                        fprintf(stderr,
                                "[repiu-frame-dump] draw bbox=%.2fx%.2f"
                                " xy=(%.2f,%.2f)(%.2f,%.2f)(%.2f,%.2f)"
                                " st=(%.2f,%.2f)(%.2f,%.2f)(%.2f,%.2f)"
                                " textured=%d tex=0x%08X texdim=%ux%u"
                                " const=0x%08X combine=%u/%u/%u/%u"
                                " blend=%u/%u\n",
                                width, height,
                                vertices[0].x, vertices[0].y,
                                vertices[1].x, vertices[1].y,
                                vertices[2].x, vertices[2].y,
                                vertices[0].s, vertices[0].t,
                                vertices[1].s, vertices[1].t,
                                vertices[2].s, vertices[2].t,
                                textured ? 1 : 0,
                                context->glide_backend.current_texture_address(),
                                context->glide_backend.current_texture_width(),
                                context->glide_backend.current_texture_height(),
                                context->glide_state.constant_color,
                                context->glide_state.color_combine.function,
                                context->glide_state.color_combine.factor,
                                context->glide_state.color_combine.local,
                                context->glide_state.color_combine.other,
                                context->glide_state.alpha_blend.rgb_source,
                                context->glide_state.alpha_blend.rgb_destination);
                    }
                    const bool is_tiny_quad = width <= 8.0F && height <= 8.0F;
                    static long tiny_samples = 0;
                    if (draw_census_enabled && is_tiny_quad &&
                        InterlockedIncrement(&tiny_samples) <= 40)
                    {
                        fprintf(stderr,
                                "[repiu-draw-census] tiny #%ld bbox=%.2fx%.2f"
                                " xy=(%.2f,%.2f)(%.2f,%.2f)(%.2f,%.2f)"
                                " st=(%.2f,%.2f)(%.2f,%.2f)(%.2f,%.2f)"
                                " textured=%d tex=0x%08X texdim=%ux%u"
                                " const=0x%08X combine=%u/%u/%u/%u\n",
                                tiny_samples, width, height,
                                vertices[0].x, vertices[0].y,
                                vertices[1].x, vertices[1].y,
                                vertices[2].x, vertices[2].y,
                                vertices[0].s, vertices[0].t,
                                vertices[1].s, vertices[1].t,
                                vertices[2].s, vertices[2].t,
                                textured ? 1 : 0,
                                context->glide_backend.current_texture_address(),
                                context->glide_backend.current_texture_width(),
                                context->glide_backend.current_texture_height(),
                                context->glide_state.constant_color,
                                context->glide_state.color_combine.function,
                                context->glide_state.color_combine.factor,
                                context->glide_state.color_combine.local,
                                context->glide_state.color_combine.other);
                    }
                    if (draw_census_enabled && (is_small_quad || small_texture))
                    {
                        if (is_small_quad)
                        {
                            ++small_draws;
                            if (!textured)
                            {
                                ++small_untextured;
                            }
                        }
                        // The first draws of a run are the title text, so a
                        // first-N sample never reaches the screen elements under
                        // investigation. Sample the opening burst and then keep
                        // sampling periodically, which reaches later screens
                        // regardless of when they arrive.
                        ++small_samples;
                        static long printed_samples = 0;
                        static long small_texture_samples = 0;
                        const bool sample_this =
                            ((small_texture &&
                              InterlockedIncrement(&small_texture_samples) <=
                                  30) ||
                             small_samples <= 20 ||
                             small_samples % 500 == 0) &&
                            printed_samples < 160;
                        if (sample_this)
                        {
                            ++printed_samples;
                            fprintf(stderr,
                                    "[repiu-draw-census] small #%ld bbox=%.1fx%.1f"
                                    " xy=(%.1f,%.1f)(%.1f,%.1f)(%.1f,%.1f)"
                                    " st=(%.1f,%.1f)(%.1f,%.1f)(%.1f,%.1f)"
                                    " textured=%d texcombine=%d tex=0x%08X"
                                    " texdim=%ux%u rgba0=(%.2f,%.2f,%.2f,%.2f)"
                                    " const=0x%08X combine=%u/%u/%u/%u"
                                    " alphatest=%u/%u blend=%u/%u/%u/%u\n",
                                    small_samples, width, height,
                                    vertices[0].x, vertices[0].y,
                                    vertices[1].x, vertices[1].y,
                                    vertices[2].x, vertices[2].y,
                                    vertices[0].s, vertices[0].t,
                                    vertices[1].s, vertices[1].t,
                                    vertices[2].s, vertices[2].t,
                                    textured ? 1 : 0,
                                    context->glide_backend
                                        .is_texture_combine_enabled() ? 1 : 0,
                                    context->glide_backend
                                        .current_texture_address(),
                                    context->glide_backend
                                        .current_texture_width(),
                                    context->glide_backend
                                        .current_texture_height(),
                                    vertices[0].r, vertices[0].g,
                                    vertices[0].b, vertices[0].a,
                                    context->glide_state.constant_color,
                                    context->glide_state.color_combine.function,
                                    context->glide_state.color_combine.factor,
                                    context->glide_state.color_combine.local,
                                    context->glide_state.color_combine.other,
                                    context->glide_state.alpha_test_function,
                                    context->glide_state.alpha_test_reference,
                                    context->glide_state.alpha_blend.rgb_source,
                                    context->glide_state.alpha_blend
                                        .rgb_destination,
                                    context->glide_state.alpha_blend
                                        .alpha_source,
                                    context->glide_state.alpha_blend
                                        .alpha_destination);
                        }
                    }
                    if (total_draws % 500 == 0)
                    {
                        fprintf(stderr,
                                "[repiu-draw-census] after %ld draws: small=%ld"
                                " small-untextured=%ld stored-textures=%u"
                                " missing-sources=%u last-missing=0x%08X\n",
                                total_draws, small_draws, small_untextured,
                                context->glide_backend.stored_texture_count(),
                                context->glide_backend
                                    .missing_texture_source_count(),
                                context->glide_backend
                                    .last_missing_texture_address());
                    }
                }
            }
            if (!QueueGlideDrawForBatch(context, draw_batch, vertices, 3U,
                                        Win32GlideBatchPrimitive::kTriangles) &&
                !context->glide_backend.DrawTriangle(vertices[0], vertices[1], vertices[2]))
            {
                context->glide_backend_message = context->glide_backend.message();
                return decline_gate("compact-triangle-backend-failure");
            }
            if (diagnose_this_draw)
            {
                std::vector<std::uint8_t> after;
                std::size_t after_non_black = 0;
                if (context->glide_backend.ReadbackFramebuffer(
                        context->glide_state.width, context->glide_state.height,
                        &after))
                {
                    for (std::size_t i = 0; i + 3U < after.size(); i += 4U)
                    {
                        if (after[i] > 8U || after[i + 1U] > 8U ||
                            after[i + 2U] > 8U)
                        {
                            ++after_non_black;
                        }
                    }
                }
                fprintf(stderr,
                        "[repiu-live-debug] tri #%u xy=(%.2f,%.2f)(%.2f,%.2f)"
                        "(%.2f,%.2f) rgba0=(%.3f,%.3f,%.3f,%.3f) st0=(%.2f,%.2f)"
                        " combine=%u/other=%u texEnabled=%d nonblack %zu->%zu\n",
                        sequence, vertices[0].x, vertices[0].y, vertices[1].x,
                        vertices[1].y, vertices[2].x, vertices[2].y,
                        vertices[0].r, vertices[0].g, vertices[0].b,
                        vertices[0].a, vertices[0].s, vertices[0].t,
                        context->glide_state.color_combine.function,
                        context->glide_state.color_combine.other,
                        context->glide_backend.is_texture_combine_enabled() ? 1 : 0,
                        before_non_black, after_non_black);
            }
            {
                // Triangle census (env-gated): aggregate every draw by the combine mode
                // and size bucket it used. The first-N sample cannot answer "why is the
                // background missing" because the background may be many small tiles
                // submitted after the UI text -- a histogram over all draws can.
                static const bool tri_census_enabled =
                    std::getenv("REPIU_GLIDE_TRI_CENSUS") != nullptr;
                if (tri_census_enabled)
                {
                    struct Bucket
                    {
                        std::uint32_t function;
                        std::uint32_t other;
                        bool textured;
                        long count;
                        float max_w;
                        float max_h;
                    };
                    static Bucket buckets[16] = {};
                    static long bucket_count = 0;
                    static long census_draws = 0;
                    const std::uint32_t fn =
                        context->glide_state.color_combine.function;
                    const std::uint32_t ot = context->glide_state.color_combine.other;
                    const bool textured =
                        context->glide_backend.is_texture_combine_enabled();
                    bool found = false;
                    for (long b = 0; b < bucket_count; ++b)
                    {
                        if (buckets[b].function == fn && buckets[b].other == ot &&
                            buckets[b].textured == textured)
                        {
                            ++buckets[b].count;
                            buckets[b].max_w = std::max(buckets[b].max_w, max_x - min_x);
                            buckets[b].max_h = std::max(buckets[b].max_h, max_y - min_y);
                            found = true;
                            break;
                        }
                    }
                    if (!found && bucket_count < 16)
                    {
                        buckets[bucket_count] = {fn, ot, textured, 1,
                                                 max_x - min_x, max_y - min_y};
                        ++bucket_count;
                    }
                    if (++census_draws % 400 == 0 && census_draws <= 4000)
                    {
                        fprintf(stderr,
                                "[repiu-tri-census] after %ld draws:\n", census_draws);
                        for (long b = 0; b < bucket_count; ++b)
                        {
                            fprintf(stderr,
                                    "    combine fn=%u other=%u textured=%d"
                                    " count=%ld max=%.0fx%.0f\n",
                                    buckets[b].function, buckets[b].other,
                                    buckets[b].textured ? 1 : 0, buckets[b].count,
                                    buckets[b].max_w, buckets[b].max_h);
                        }
                    }
                }
            }
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 4U * sizeof(std::uint32_t);
            return true;
        }

        // Task 420: both polygon forms render as a triangle fan. Glide requires
        // the vertices to describe a convex polygon, so the fan is exact rather
        // than an approximation. The indexed form gathers through `ilist`; the
        // vertex-list form walks a contiguous array at the producer stride.
        case go::kGrDrawPlanarPolygon: // _GRDRAWPLANARPOLYGON@12
        case go::kGrDrawPolygon: // _GRDRAWPOLYGON@12
        case go::kGrAADrawPolygon: // _GRAADRAWPOLYGON@12
        case go::kGrDrawPlanarPolygonVertexList: // _GRDRAWPLANARPOLYGONVERTEXLIST@8
        case go::kGrDrawPolygonVertexList: // _GRDRAWPOLYGONVERTEXLIST@8
        case go::kGrAADrawPolygonVertexList: // _GRAADRAWPOLYGONVERTEXLIST@8
        {
            const bool indexed =
                glide_export->gate_id == go::kGrDrawPlanarPolygon ||
                glide_export->gate_id == go::kGrDrawPolygon ||
                glide_export->gate_id == go::kGrAADrawPolygon;
            if (!draw_entry_points_enabled)
            {
                record_unimplemented(
                    indexed ? "draw-polygon-noop"
                            : "draw-polygon-vertex-list-noop",
                    "draw request accepted without rendering");
                ++context->glide_gate_handled_count;
                win32_context->Eip = return_address;
                win32_context->Esp +=
                    (indexed ? 4U : 3U) * sizeof(std::uint32_t);
                return true;
            }
            const std::uint32_t vertex_count = context->glide_gate_stack[1];
            const std::uint32_t index_list =
                indexed ? context->glide_gate_stack[2] : 0U;
            const std::uint32_t vertex_list =
                context->glide_gate_stack[indexed ? 3U : 2U];
            // The stack adjustment must not depend on whether the draw
            // succeeded, so it is computed once here and applied on every exit.
            const std::uint32_t stack_slots = indexed ? 4U : 3U;

            if (vertex_count < 3U ||
                vertex_count > hle::kMaxGlidePolygonVertices)
            {
                return decline_gate("draw-polygon-vertex-count");
            }
            const auto* indices = reinterpret_cast<const std::int32_t*>(
                static_cast<std::uintptr_t>(index_list));
            if (indexed &&
                !IsGuestRangeReadable(context, indices,
                                      vertex_count * sizeof(std::int32_t)))
            {
                return decline_gate("draw-polygon-unreadable-index-list");
            }

            hle::GlideDrawVertex vertices[hle::kMaxGlidePolygonVertices] = {};
            for (std::uint32_t index = 0U; index < vertex_count; ++index)
            {
                std::int32_t slot = static_cast<std::int32_t>(index);
                if (indexed)
                {
                    std::memcpy(&slot, &indices[index], sizeof(slot));
                    if (slot < 0)
                    {
                        return decline_gate("draw-polygon-negative-index");
                    }
                }
                const auto* source = reinterpret_cast<const std::uint32_t*>(
                    static_cast<std::uintptr_t>(vertex_list) +
                    static_cast<std::uintptr_t>(
                        static_cast<std::uint32_t>(slot)) *
                        hle::kGlideProducerVertexByteCount);
                if (!IsGuestRangeReadable(context, source,
                                          hle::kGlideProducerVertexByteCount))
                {
                    return decline_gate("draw-polygon-unreadable-vertex");
                }
                std::uint32_t producer[
                    hle::kGlideProducerVertexDwordCount] = {};
                std::memcpy(producer, source, sizeof(producer));
                if (!hle::DecodeGlideProducerVertex(
                        producer, hle::kGlideProducerVertexDwordCount,
                        &vertices[index]))
                {
                    return decline_gate("draw-polygon-decode-failure");
                }
            }
            if (!context->glide_backend.DrawPolygon(vertices, vertex_count))
            {
                context->glide_backend_message =
                    context->glide_backend.message();
                return decline_gate("draw-polygon-backend-failure");
            }
            context->glide_backend_message = context->glide_backend.message();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += stack_slots * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrLfbLock: // _GRLFBLOCK@24
        {
            // grLfbLock(type, buffer, writeMode, origin, pixelPipeline, info*).
            // The observed content-phase lock is (WRITE_ONLY, BACKBUFFER, 565,
            // UPPER_LEFT, FXTRUE), which is exactly what this path implements.
            const std::uint32_t type = context->glide_gate_stack[1];
            const std::uint32_t buffer = context->glide_gate_stack[2];
            const std::uint32_t write_mode = context->glide_gate_stack[3];
            const std::uint32_t origin = context->glide_gate_stack[4];
            const std::uint32_t info_pointer = context->glide_gate_stack[6];
            auto* info = reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(info_pointer));

            const auto fail_lock = [&](const char* reason) {
                // Retain policy (design 237): an unsupported but ABI-valid lock
                // returns FXFALSE rather than leaking the frame through a reject.
                RecordGlideImplementationIssue(
                    win32_context,
                    context,
                    *glide_export,
                    repiu::hle::GlideImplementationIssueKind::
                        kUnsupportedArgument,
                    reason,
                    "grLfbLock returns FXFALSE",
                    "continue");
                ++context->glide_gate_handled_count;
                win32_context->Eax = 0U;
                win32_context->Eip = return_address;
                win32_context->Esp += 7U * sizeof(std::uint32_t);
                return true;
            };

            if (!IsGuestRangeWritable(context, info,
                                      repiu::hle::kGlideLfbInfoByteCount))
            {
                return decline_gate("lfb-lock-info-pointer-unwritable");
            }
            if (write_mode != repiu::hle::kGlideLfbWriteMode565 ||
                (buffer != repiu::hle::kGlideBufferBackBuffer &&
                 buffer != repiu::hle::kGlideBufferFrontBuffer))
            {
                return fail_lock("unsupported-writemode-or-buffer");
            }
            const std::uint32_t width = context->glide_state.width;
            const std::uint32_t height = context->glide_state.height;
            if (!context->glide_lfb_surface.Resize(width, height))
            {
                return fail_lock("surface-resize-failure");
            }
            if (!context->glide_lfb_surface.BeginLock(type, buffer, write_mode,
                                                      origin))
            {
                return fail_lock("lock-already-outstanding");
            }
            {
                // Seed the staging surface from the current render target for *every*
                // lock, not just read locks. On real hardware the LFB is the live
                // framebuffer, so a write lock that touches only some pixels leaves the
                // rest untouched. Handing out a zero-filled buffer instead makes unlock
                // blit black over everything already drawn -- observed erasing the
                // triangles submitted immediately before the lock.
                std::vector<std::uint8_t> rgba8;
                const bool read_ok = context->glide_backend.ReadbackFramebuffer(
                    width, height, &rgba8);
                bool encode_ok = false;
                if (read_ok)
                {
                    const std::uint32_t lfb_color_format =
                        type == repiu::hle::kGlideLfbWriteOnly
                            ? context->glide_state.lfb_write_color_format
                            : context->glide_state.color_format;
                    encode_ok = repiu::hle::EncodeRgba8ToGlideLfb565(
                        rgba8.data(), rgba8.size(), width, height,
                        lfb_color_format,
                        context->glide_lfb_surface.pixels(),
                        context->glide_lfb_surface.byte_count());
                }
                static long seed_log_count = 0;
                const long seed_index = InterlockedIncrement(&seed_log_count);
                if (seed_index <= 4)
                {
                    std::size_t seed_non_black = 0;
                    for (std::size_t i = 0; i + 3U < rgba8.size(); i += 4U)
                    {
                        if (rgba8[i] > 8U || rgba8[i + 1U] > 8U ||
                            rgba8[i + 2U] > 8U)
                        {
                            ++seed_non_black;
                        }
                    }
                    fprintf(stderr,
                            "[repiu-live-debug] grLfbLock seed #%ld read=%d"
                            " encode=%d framebuffer non-black=%zu\n",
                            seed_index, read_ok ? 1 : 0, encode_ok ? 1 : 0,
                            seed_non_black);
                }
            }

            // The size field is caller-supplied; echo it back but record a mismatch
            // so an unexpected GrLfbInfo_t layout becomes visible instead of silent.
            std::uint32_t caller_size = 0;
            std::memcpy(&caller_size, info, sizeof(caller_size));
            {
                static long lfb_size_log_count = 0;
                const long index = InterlockedIncrement(&lfb_size_log_count);
                if (index <= 4)
                {
                    // Dump what the caller staged in the struct before we touch it.
                    // Glide 2.4's GrLfbInfo_t starts with a caller-set `size`; PIU
                    // leaves offset 0 at zero, so this dump is what distinguishes "the
                    // game simply never sets size" from "this build's GrLfbInfo_t has
                    // no size field and every field we write is off by one dword".
                    std::uint32_t before[8] = {};
                    const auto* raw = reinterpret_cast<const std::uint32_t*>(
                        static_cast<std::uintptr_t>(info_pointer));
                    if (IsGuestRangeReadable(context, raw, sizeof(before)))
                    {
                        std::memcpy(before, raw, sizeof(before));
                    }
                    fprintf(stderr,
                            "[repiu-live-debug] grLfbLock GrLfbInfo_t caller size=%u"
                            " (expected %u) pre-call dwords="
                            "%08X %08X %08X %08X %08X %08X %08X %08X\n",
                            caller_size,
                            repiu::hle::kGlideLfbInfoExpectedSize,
                            before[0], before[1], before[2], before[3],
                            before[4], before[5], before[6], before[7]);
                }
            }

            // grLfbLock owns every output field, `size` included: it reports the
            // GrLfbInfo_t layout it actually filled. Echoing the caller's value back
            // (PIU leaves it 0) would tell the guest nothing was written.
            std::uint8_t image[repiu::hle::kGlideLfbInfoByteCount] = {};
            const auto staging_pointer = static_cast<std::uint32_t>(
                reinterpret_cast<std::uintptr_t>(
                    context->glide_lfb_surface.pixels()));
            if (!repiu::hle::BuildGlideLfbInfoImage(
                    repiu::hle::kGlideLfbInfoExpectedSize, staging_pointer,
                    context->glide_lfb_surface.stride_in_bytes(), write_mode,
                    origin, image, sizeof(image)) ||
                !WriteGuestBytes(context, info, image, sizeof(image)))
            {
                context->glide_lfb_surface.EndLock();
                return fail_lock("info-write-failure");
            }

            ++context->glide_lfb_lock_count;
            if (context->glide_lfb_lock_count <= 4U)
            {
                fprintf(stderr,
                        "[repiu-live-debug] grLfbLock granted #%u type=%u"
                        " buffer=%u writeMode=%u origin=%u lfbPtr=0x%08X"
                        " stride=%u %ux%u\n",
                        context->glide_lfb_lock_count, type, buffer, write_mode,
                        origin, staging_pointer,
                        context->glide_lfb_surface.stride_in_bytes(), width,
                        height);
            }
            ++context->glide_gate_handled_count;
            win32_context->Eax = 1U;
            win32_context->Eip = return_address;
            win32_context->Esp += 7U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrLfbUnlock: // _GRLFBUNLOCK@8
        {
            // grLfbUnlock(type, buffer): a write lock's staging content becomes the
            // back-buffer image here; the next grBufferSwap presents it.
            const std::uint32_t type = context->glide_gate_stack[1];
            if (context->glide_lfb_surface.locked() &&
                type == repiu::hle::kGlideLfbWriteOnly)
            {
                {
                    // Did the guest actually write into the surface we handed it? A
                    // non-zero count proves the lfbPtr round-trip works end to end and
                    // separates "blit is broken" from "guest never wrote".
                    static long unlock_probe_count = 0;
                    const long probe = InterlockedIncrement(&unlock_probe_count);
                    if (probe <= 8)
                    {
                        const std::uint8_t* pixels =
                            context->glide_lfb_surface.pixels();
                        const std::size_t total =
                            context->glide_lfb_surface.byte_count();
                        std::size_t non_zero = 0;
                        for (std::size_t i = 0; i < total; ++i)
                        {
                            if (pixels[i] != 0U)
                            {
                                ++non_zero;
                            }
                        }
                        fprintf(stderr,
                                "[repiu-live-debug] grLfbUnlock #%ld non-zero"
                                " staging bytes=%zu/%zu first-texels="
                                "%02X%02X %02X%02X %02X%02X\n",
                                probe, non_zero, total, pixels[1], pixels[0],
                                pixels[3], pixels[2], pixels[5], pixels[4]);
                    }
                }
                std::vector<std::uint8_t> rgba8;
                if (repiu::hle::DecodeGlideLfb565ToRgba8(
                        context->glide_lfb_surface.pixels(),
                        context->glide_lfb_surface.byte_count(),
                        context->glide_lfb_surface.width(),
                        context->glide_lfb_surface.height(),
                        context->glide_state.lfb_write_color_format, &rgba8))
                {
                    const char* lfb_dump = std::getenv("REPIU_DUMP_LFB_BMP");
                    if (lfb_dump != nullptr && lfb_dump[0] == '1')
                    {
                        DumpLfbSurfaceToBmp(
                            0x1FB,
                            0,
                            context->glide_lfb_surface.width(),
                            context->glide_lfb_surface.height(),
                            rgba8);
                    }
                    const bool flip_v =
                        context->glide_lfb_surface.lock_origin() ==
                        repiu::hle::kGlideOriginLowerLeft;
                    const bool present_to_front =
                        context->glide_lfb_surface.lock_buffer() ==
                        repiu::hle::kGlideBufferFrontBuffer;
                    const bool presented =
                        context->glide_backend.PresentLfbSurface(
                            rgba8.data(), context->glide_lfb_surface.width(),
                            context->glide_lfb_surface.height(), flip_v,
                            present_to_front);
                    context->glide_backend_message =
                        context->glide_backend.message();
                    if (presented)
                    {
                        ++context->glide_lfb_present_count;
                        if (context->glide_lfb_present_count <= 4U)
                        {
                            // The swap-driven pixel diagnostic cannot help here: the
                            // guest may never swap again. Sample the back buffer right
                            // after the blit so the blit itself is verifiable.
                            std::vector<std::uint8_t> after;
                            if (context->glide_backend.ReadbackFramebuffer(
                                    context->glide_lfb_surface.width(),
                                    context->glide_lfb_surface.height(), &after))
                            {
                                std::size_t non_black = 0;
                                std::size_t non_zero = 0;
                                std::uint8_t maximum = 0U;
                                for (std::size_t i = 0; i + 3U < after.size();
                                     i += 4U)
                                {
                                    if (after[i] > 8U || after[i + 1U] > 8U ||
                                        after[i + 2U] > 8U)
                                    {
                                        ++non_black;
                                    }
                                    if (after[i] != 0U || after[i + 1U] != 0U ||
                                        after[i + 2U] != 0U)
                                    {
                                        ++non_zero;
                                    }
                                    maximum = (std::max)(maximum, after[i]);
                                    maximum = (std::max)(maximum, after[i + 1U]);
                                    maximum = (std::max)(maximum, after[i + 2U]);
                                }
                                fprintf(stderr,
                                        "[repiu-live-debug] LFB blit #%u back-buffer"
                                        " visible=%zu/%zu nonzero=%zu max=%u\n",
                                        context->glide_lfb_present_count, non_black,
                                        after.size() / 4U, non_zero, maximum);
                            }
                        }
                    }
                    else
                    {
                        record_backend_failure(
                            "lfb-unlock-present-backend-failure",
                            context->glide_backend_message);
                    }
                }
                else
                {
                    record_backend_failure(
                        "lfb-unlock-decode-failure",
                        "565 staging surface could not be decoded");
                }
            }
            else
            {
                record_unsupported(
                    "lfb-unlock-state-unsupported",
                    "no matching write lock is outstanding");
            }
            context->glide_lfb_surface.EndLock();
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 3U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrLfbWriteRegion: // _GRLFBWRITEREGION@32
        {
            const auto arguments = CaptureGlideImplementationIssueArguments(
                win32_context, context, glide_export->argument_byte_count);
            const std::uint32_t dst_buffer = context->glide_gate_stack[1];
            const std::uint32_t dst_x = context->glide_gate_stack[2];
            const std::uint32_t dst_y = context->glide_gate_stack[3];
            const std::uint32_t src_format = context->glide_gate_stack[4];
            const std::uint32_t src_width = context->glide_gate_stack[5];
            const std::uint32_t src_height = context->glide_gate_stack[6];
            const std::int32_t src_stride = static_cast<std::int32_t>(context->glide_gate_stack[7]);
            const std::uint32_t src_data_ptr = arguments[7];

            // The region write lands in the frame buffer shadow, not on the
            // screen: presenting a whole surface per row would both destroy the
            // untouched pixels and cost one full-screen upload per scanline.
            // The shadow reaches the backend at the next non-region gate.
            const std::size_t data_size = ResolveGlideLfbRegionSpan(
                src_width, src_height, src_format, src_stride);
            const auto* guest_ptr = reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(src_data_ptr));
            const char* decline = ClassifyGlideLfbRegionArguments(
                context, dst_buffer, src_width, src_height, src_stride,
                src_format);
            if (decline != nullptr)
            {
                record_unsupported("lfb-write-region-unsupported", decline);
            }
            else if (data_size == 0U)
            {
                record_unsupported(
                    "lfb-write-region-unsupported",
                    "source geometry does not describe any pixels");
            }
            else if (!IsGuestRangeReadable(
                         context, guest_ptr,
                         static_cast<std::uint32_t>(data_size)))
            {
                record_backend_failure(
                    "lfb-write-region-unreadable-memory",
                    "source region is not guest-readable");
            }
            else if (!EnsureGlideLfbRegionShadow(context, dst_buffer))
            {
                record_backend_failure(
                    "lfb-write-region-shadow-failure",
                    "frame buffer shadow could not be prepared");
            }
            else
            {
                std::vector<std::uint8_t> host_src(data_size);
                std::memcpy(host_src.data(), guest_ptr, data_size);
                if (!repiu::hle::WriteGlideLfbRegion(
                        dst_x, dst_y, src_width, src_height, src_format,
                        src_stride, host_src.data(), host_src.size(),
                        context->glide_state.lfb_write_color_format,
                        context->glide_state.color_format,
                        &context->glide_lfb_surface))
                {
                    record_unsupported(
                        "lfb-write-region-geometry-unsupported",
                        "region lies outside the frame buffer");
                }
                else
                {
                    context->glide_lfb_region_shadow_dirty = true;
                    ++context->glide_lfb_region_write_count;
                }
            }
            ++context->glide_gate_handled_count;
            // FXTRUE even when declined, preserving the work-order 002 finding
            // that a false return from the LFB family stalls the guest.
            win32_context->Eax = 1U;
            win32_context->Eip = return_address;
            win32_context->Esp += 9U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrLfbReadRegion: // _GRLFBREADREGION@28
        {
            // grLfbReadRegion(buffer, x, y, width, height, dstStride, dstData)
            // hands back frame buffer pixels in the buffer's native 565 form.
            const auto arguments = CaptureGlideImplementationIssueArguments(
                win32_context, context, glide_export->argument_byte_count);
            const std::uint32_t src_buffer = context->glide_gate_stack[1];
            const std::uint32_t src_x = context->glide_gate_stack[2];
            const std::uint32_t src_y = context->glide_gate_stack[3];
            const std::uint32_t src_width = context->glide_gate_stack[4];
            const std::uint32_t src_height = context->glide_gate_stack[5];
            const std::int32_t dst_stride =
                static_cast<std::int32_t>(context->glide_gate_stack[6]);
            const std::uint32_t dst_data_ptr = arguments[6];

            const std::size_t data_size = ResolveGlideLfbRegionSpan(
                src_width, src_height, repiu::hle::kGlideLfbSrcFmt565,
                dst_stride);
            auto* guest_ptr = reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(dst_data_ptr));
            const char* decline = ClassifyGlideLfbRegionArguments(
                context, src_buffer, src_width, src_height, dst_stride,
                repiu::hle::kGlideLfbSrcFmt565);
            if (decline != nullptr)
            {
                record_unsupported("lfb-read-region-unsupported", decline);
            }
            else if (data_size == 0U)
            {
                record_unsupported(
                    "lfb-read-region-unsupported",
                    "destination geometry does not describe any pixels");
            }
            else if (!IsGuestRangeWritable(
                         context, guest_ptr,
                         static_cast<std::uint32_t>(data_size)))
            {
                record_backend_failure(
                    "lfb-read-region-unwritable-memory",
                    "destination region is not guest-writable");
            }
            else if (!EnsureGlideLfbRegionShadow(context, src_buffer))
            {
                record_backend_failure(
                    "lfb-read-region-shadow-failure",
                    "frame buffer shadow could not be prepared");
            }
            else
            {
                std::vector<std::uint8_t> host_dst(data_size);
                std::uint32_t copied_width = 0;
                std::uint32_t copied_height = 0;
                const std::size_t row_pitch = static_cast<std::size_t>(
                    repiu::hle::ResolveGlideLfbRegionStride(
                        src_width, repiu::hle::kGlideLfb565BytesPerTexel,
                        dst_stride));
                if (!repiu::hle::ReadGlideLfbRegion(
                        src_x, src_y, src_width, src_height, dst_stride,
                        context->glide_lfb_surface, host_dst.data(),
                        host_dst.size(), &copied_width, &copied_height))
                {
                    record_unsupported(
                        "lfb-read-region-geometry-unsupported",
                        "region lies outside the frame buffer");
                }
                else
                {
                    // Copy row by row so a destination stride wider than the
                    // rectangle keeps whatever the guest put in the padding.
                    const std::size_t row_bytes =
                        static_cast<std::size_t>(copied_width) *
                        repiu::hle::kGlideLfb565BytesPerTexel;
                    bool written = true;
                    for (std::uint32_t row = 0; row < copied_height && written;
                         ++row)
                    {
                        const std::size_t offset =
                            static_cast<std::size_t>(row) * row_pitch;
                        written = WriteGuestBytes(
                            context,
                            reinterpret_cast<void*>(
                                static_cast<std::uintptr_t>(dst_data_ptr) +
                                offset),
                            host_dst.data() + offset, row_bytes);
                    }
                    if (!written)
                    {
                        record_backend_failure(
                            "lfb-read-region-write-failure",
                            "destination region could not be written");
                    }
                    else
                    {
                        ++context->glide_lfb_region_read_count;
                    }
                }
            }
            ++context->glide_gate_handled_count;
            win32_context->Eax = 1U;
            win32_context->Eip = return_address;
            win32_context->Esp += 8U * sizeof(std::uint32_t);
            return true;
        }

        case go::kGrLfbConstantAlpha: // _GRLFBCONSTANTALPHA@4
        case go::kGrLfbConstantDepth: // _GRLFBCONSTANTDEPTH@4
            record_unimplemented(
                "lfb-constant-state-noop",
                "state accepted without implementation");
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 2U * sizeof(std::uint32_t);
            return true;

        case go::kGrLfbWriteColorSwizzle: // _GRLFBWRITECOLORSWIZZLE@8
            record_unimplemented(
                "lfb-color-swizzle-noop",
                "state accepted without implementation");
            ++context->glide_gate_handled_count;
            win32_context->Eip = return_address;
            win32_context->Esp += 3U * sizeof(std::uint32_t);
            return true;

        default:
            break;
    }
    // Default handler for unhandled but cataloged gates (Phase R0)
    record_unimplemented(
        "catalog-default-handler",
        "ABI-preserving default return");
    ++context->glide_gate_handled_count;
    if (signature->return_kind != repiu::hle::GlideReturnKind::kVoid)
    {
        win32_context->Eax = 0U;
    }
    win32_context->Eip = return_address;
    win32_context->Esp += sizeof(std::uint32_t) + signature->argument_byte_count;

    return true;
}

// Lightweight VEH transfer paths run without an ExceptionDispatchScope, so
// these counters must be mirrored into shared telemetry here to stay
// externally observable during dispatch-silent phases.

} // namespace repiu::platform::win32
