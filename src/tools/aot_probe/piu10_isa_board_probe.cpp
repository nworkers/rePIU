#include "piu10_isa_board_probe.h"

#include "repiu/hle/piu10_isa_board.h"
#include "repiu/platform/win32/piu10_mp3_audio_out.h"
#include "repiu/sound/decoder_input_fifo.h"
#include "repiu/sound/mpeg_audio_frame.h"
#include "repiu/sound/stream_chunk_audit.h"
#include "repiu/target/target_profile.h"
#include "execution/thread_context.h"
#include "io/piu10_mp3_frame_batch.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace repiu::tools
{

bool RunPiu10IsaBoardProbe()
{
    bool target_profiles_valid = true;
    bool jamma_target_profiles_valid = true;
    for (const char* id : {"dos4gw_hello", "piu_1st"})
    {
        const target::TargetProfile* profile =
            target::FindTargetProfileById(id);
        jamma_target_profiles_valid = jamma_target_profiles_valid &&
            profile != nullptr && !profile->enable_piu_jamma_board;
    }
    for (const char* id : {"pumpit1", "pumpit2", "pumpit3", "pumpito",
                           "pumpitc", "pumpitpc", "pumpite"})
    {
        const target::TargetProfile* profile =
            target::FindTargetProfileById(id);
        jamma_target_profiles_valid = jamma_target_profiles_valid &&
            profile != nullptr && profile->enable_piu_jamma_board;
    }
    for (const char* id : {"pumpit1", "pumpit2", "pumpit3"})
    {
        const target::TargetProfile* profile =
            target::FindTargetProfileById(id);
        target_profiles_valid = target_profiles_valid && profile != nullptr &&
            !profile->enable_piu10_isa_board;
    }
    for (const char* id : {"pumpito", "pumpitc", "pumpitpc", "pumpite"})
    {
        const target::TargetProfile* profile =
            target::FindTargetProfileById(id);
        target_profiles_valid = target_profiles_valid && profile != nullptr &&
            profile->enable_piu10_isa_board;
    }
    const target::TargetProfile* pumpito_profile =
        target::FindTargetProfileById("pumpito");
    bool mp3_latency_profile_valid =
        pumpito_profile != nullptr &&
        pumpito_profile->piu10_mp3_latency_ms == 0U;
    for (const char* id : {
             "pumpit1", "pumpit2", "pumpit3", "pumpitc", "pumpitpc",
             "pumpite"})
    {
        const target::TargetProfile* profile =
            target::FindTargetProfileById(id);
        mp3_latency_profile_valid = mp3_latency_profile_valid &&
            profile != nullptr && profile->piu10_mp3_latency_ms == 0U;
    }

    std::vector<std::uint8_t> flash(hle::Piu10IsaBoard::kFlashBytes, 0xFFU);
    flash[0x2468U] = 0x34U;
    flash[0x2469U] = 0x12U;
    flash[0x246AU] = 0x78U;
    flash[0x246BU] = 0x56U;
    const std::array<std::uint8_t, 8> transform = {
        0x5AU, 0xA5U, 0x3CU, 0xC3U, 0x96U, 0x69U, 0xF0U, 0x0FU};

    const std::array<std::uint8_t, 4> mpeg_header = {
        0xFFU, 0xFBU, 0x90U, 0x64U};
    sound::MpegAudioFrameInfo mpeg_info;
    const bool mpeg_parser_valid =
        sound::ParseMpegAudioFrameHeader(mpeg_header, &mpeg_info) &&
        mpeg_info.frame_bytes == 417U &&
        mpeg_info.sample_rate_hz == 44100U &&
        mpeg_info.bitrate_kbps == 128U &&
        mpeg_info.samples_per_frame == 1152U && mpeg_info.channels == 2U;

    hle::Piu10IsaBoard board;
    std::string message;
    if (!board.Initialize(std::move(flash), transform, &message))
    {
        std::cout << "piu10_isa_board_probe=false,message=" << message << "\n";
        return false;
    }

    std::uint16_t value = 0;
    const bool status_valid =
        board.Write16(0x02D4U, 0x0080U) &&
        board.Write16(0x02D6U, 0x0000U) &&
        board.Read16(0x02DAU, &value) && (value & 0x0007U) == 0x0007U;
    board.SetMp3StatusSource([]() { return std::uint8_t{0x04U}; });
    const bool status_source_valid =
        board.Read16(0x02DAU, &value) && (value & 0x0007U) == 0x0006U;

    const bool flash_valid =
        board.Write16(0x02D0U, 0x0034U) &&
        board.Write16(0x02D2U, 0x0012U) &&
        board.Write16(0x02D4U, 0x0000U) &&
        board.Write16(0x02D6U, 0x0000U) &&
        board.Write16(0x02DCU, 0x0008U) &&
        board.Read16(0x02DAU, &value) && value == 0x1234U &&
        board.Read16(0x02DAU, &value) && value == 0x5678U &&
        board.address() == 0x1236U;

    std::vector<std::uint8_t> mp3_bytes;
    board.SetMp3DataSink([&mp3_bytes](std::uint8_t byte) {
        mp3_bytes.push_back(byte);
    });
    const bool mp3_stream_valid =
        board.Write16(0x02D4U, 0x0080U) &&
        board.Write16(0x02D6U, 0x0000U) &&
        board.Write8(0x02DAU, 0xA5U) &&
        board.Write16(0x02DAU, 0x12C3U) &&
        mp3_bytes == std::vector<std::uint8_t>({0xA5U, 0xC3U});

    std::vector<sound::Dac3350aControlEvent> dac_events;
    board.SetDacControlSink(
        [&dac_events](const sound::Dac3350aControlEvent& event) {
            dac_events.push_back(event);
        });
    board.Write16(0x02D4U, 0x0100U);
    board.Write16(0x02D6U, 0x0001U);
    const auto write_dac_lines = [&board](bool data, bool clock) {
        return board.Write16(
            0x02DAU, static_cast<std::uint16_t>(
                (data ? 0x02U : 0U) | (clock ? 0x01U : 0U)));
    };
    bool dac_data = true;
    const auto set_dac_lines = [&write_dac_lines, &dac_data](
        bool data, bool clock) {
        dac_data = data;
        return write_dac_lines(data, clock);
    };
    const auto write_dac_byte = [
        &set_dac_lines, &write_dac_lines, &dac_data](std::uint8_t byte) {
        for (int bit = 7; bit >= 0; --bit)
        {
            const bool data = ((byte >> bit) & 1U) != 0U;
            write_dac_lines(dac_data, false);
            set_dac_lines(data, false);
            write_dac_lines(dac_data, true);
        }
        write_dac_lines(dac_data, false);
        set_dac_lines(true, false);
        write_dac_lines(dac_data, true);
        write_dac_lines(dac_data, false);
    };
    set_dac_lines(true, true);
    set_dac_lines(false, true);
    for (const std::uint8_t byte : {0x9AU, 0x02U, 0x00U, 0x00U})
    {
        write_dac_byte(byte);
    }
    write_dac_lines(dac_data, false);
    set_dac_lines(false, false);
    write_dac_lines(dac_data, true);
    set_dac_lines(true, true);
    const bool dac_control_valid =
        dac_events.size() == 1U && dac_events[0].analog_volume &&
        dac_events[0].data_bytes == 2U && dac_events[0].data == 0U &&
        dac_events[0].left_gain == 0.0F &&
        dac_events[0].right_gain == 0.0F && dac_events[0].stereo_muted;
    const auto gain_near = [](float actual, float expected) {
        return std::fabs(actual - expected) <= expected * 0.0001F + 0.000001F;
    };
    const bool dac_gain_valid =
        sound::Dac3350aControl::CalculateAnalogGain(0U) == 0.0F &&
        gain_near(sound::Dac3350aControl::CalculateAnalogGain(1U),
                  0.000177828F) &&
        gain_near(sound::Dac3350aControl::CalculateAnalogGain(8U),
                  0.001995262F) &&
        gain_near(sound::Dac3350aControl::CalculateAnalogGain(44U), 1.0F) &&
        gain_near(sound::Dac3350aControl::CalculateAnalogGain(63U),
                  7.943282F);
    const bool mp3_latency_bytes_valid =
        platform::win32::Piu10Mp3AudioOut::CalculateStartupSilenceBytes(
            0U, 44100, 2) == 0U &&
        platform::win32::Piu10Mp3AudioOut::CalculateStartupSilenceBytes(
            50U, 44100, 2) == 8820U;
    platform::win32::Piu10Mp3AudioOut unopened_audio;
    const platform::win32::Piu10Mp3AudioSnapshot unopened_snapshot =
        unopened_audio.Snapshot();
    const bool mp3_snapshot_valid =
        !unopened_snapshot.available &&
        unopened_snapshot.pcm_queued_bytes == -1 &&
        unopened_snapshot.pcm_queued_ms == 0.0 &&
        unopened_snapshot.compressed_ring_bytes == 0U &&
        unopened_snapshot.decoder_pending_bytes == 0U &&
        unopened_snapshot.compressed_inflight_bytes == 0U &&
        unopened_snapshot.received_bytes == 0U &&
        unopened_snapshot.decoded_frames == 0U;

    sound::DecoderInputFifo decoder_fifo(
        platform::win32::Piu10Mp3AudioOut::kCompressedFifoBytes, 4096U);
    std::vector<std::uint8_t> logical_fill(
        platform::win32::Piu10Mp3AudioOut::kCompressedFifoBytes, 0x5AU);
    bool ring_valid =
        decoder_fifo.PushBatch(logical_fill) == logical_fill.size();
    const bool demand_deasserted =
        decoder_fifo.inflight_size() ==
            platform::win32::Piu10Mp3AudioOut::kCompressedFifoBytes &&
        !decoder_fifo.demand();
    std::array<std::uint8_t, 512> first_ring_read = {};
    ring_valid = ring_valid &&
        decoder_fifo.Pop(first_ring_read) == first_ring_read.size() &&
        std::all_of(first_ring_read.begin(), first_ring_read.end(),
                    [](std::uint8_t value) { return value == 0x5AU; });
    const bool demand_stays_low_after_pop =
        decoder_fifo.inflight_size() ==
            platform::win32::Piu10Mp3AudioOut::kCompressedFifoBytes &&
        !decoder_fifo.demand();
    ring_valid = ring_valid && decoder_fifo.Consume(417U);
    const bool demand_reasserted =
        decoder_fifo.inflight_size() ==
            platform::win32::Piu10Mp3AudioOut::kCompressedFifoBytes - 417U &&
        decoder_fifo.demand();
    std::array<std::uint8_t, 512> logical_refill = {};
    const bool batch_refill_valid =
        decoder_fifo.PushBatch(logical_refill) == 417U &&
        !decoder_fifo.demand();
    bool physical_headroom_valid = true;
    for (std::size_t index = 0U; index < 512U; ++index)
    {
        physical_headroom_valid = physical_headroom_valid &&
            decoder_fifo.PushByte(static_cast<std::uint8_t>(index));
    }
    physical_headroom_valid = physical_headroom_valid &&
        decoder_fifo.inflight_size() == 4096U &&
        !decoder_fifo.PushByte(0xFFU);
    std::array<std::uint8_t, 4096> second_ring_read = {};
    const std::size_t second_ring_count =
        decoder_fifo.Pop(second_ring_read);
    ring_valid = ring_valid &&
        second_ring_count == 4001U && decoder_fifo.Consume(4096U) &&
        decoder_fifo.inflight_size() == 0U && decoder_fifo.demand();
    ring_valid = ring_valid && demand_deasserted &&
        demand_stays_low_after_pop && demand_reasserted &&
        batch_refill_valid && physical_headroom_valid &&
        decoder_fifo.ring_size() == 0U &&
        decoder_fifo.inflight_high_water() == 4096U;

    constexpr std::size_t kSyntheticArenaBytes = 0x00300000U;
    std::vector<std::uint8_t> synthetic_arena(kSyntheticArenaBytes, 0U);
    auto batch_context =
        std::make_unique<platform::win32::ThreadContext>();
    batch_context->runtime_base = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(synthetic_arena.data()));
    batch_context->runtime_size =
        static_cast<std::uint32_t>(synthetic_arena.size());
    batch_context->piu10_mp3_frame_batch_enabled = true;
    constexpr std::uint32_t kBatchOutOffset = 0x00034567U;
    constexpr std::uint32_t kCursorOffset = 0x00201020U;
    constexpr std::uint32_t kAvailableEndOffset = 0x00201024U;
    constexpr std::uint32_t kFrameTargetOffset = 0x00201018U;
    constexpr std::uint32_t kFrameCountOffset = 0x0020101CU;
    constexpr std::uint32_t kSourceBufferOffset = 0x00202038U;
    auto* batch_out = synthetic_arena.data() + kBatchOutOffset;
    const auto runtime_address = [&batch_context](std::uint32_t offset) {
        return batch_context->runtime_base + offset;
    };
    const auto write_u32 = [](std::uint8_t* destination,
                              std::uint32_t source) {
        std::memcpy(destination, &source, sizeof(source));
    };
    const std::uint32_t cursor_address = runtime_address(kCursorOffset);
    const std::uint32_t available_end_address =
        runtime_address(kAvailableEndOffset);
    const std::uint32_t frame_target_address =
        runtime_address(kFrameTargetOffset);
    const std::uint32_t frame_count_address =
        runtime_address(kFrameCountOffset);
    const std::uint32_t source_buffer_address =
        runtime_address(kSourceBufferOffset);
    batch_out[-35] = 0xA1U;
    write_u32(batch_out - 34, cursor_address);
    batch_out[-30] = 0x8BU;
    batch_out[-29] = 0x2DU;
    write_u32(batch_out - 28, frame_count_address);
    batch_out[-24] = 0x8DU;
    batch_out[-23] = 0x50U;
    batch_out[-22] = 0x01U;
    batch_out[-21] = 0x45U;
    batch_out[-20] = 0x89U;
    batch_out[-19] = 0x15U;
    write_u32(batch_out - 18, cursor_address);
    batch_out[-14] = 0x8AU;
    batch_out[-13] = 0x80U;
    write_u32(batch_out - 12, source_buffer_address);
    batch_out[-8] = 0x89U;
    batch_out[-7] = 0xF2U;
    batch_out[-6] = 0x89U;
    batch_out[-5] = 0x2DU;
    write_u32(batch_out - 4, frame_count_address);
    batch_out[0] = 0xEEU;
    batch_out[1] = 0xA1U;
    write_u32(batch_out + 2, frame_count_address);
    batch_out[6] = 0x8BU;
    batch_out[7] = 0x15U;
    write_u32(batch_out + 8, frame_target_address);
    const std::array<std::uint8_t, 9> batch_suffix = {
        0x41U, 0x39U, 0xD0U, 0x0FU, 0x85U,
        0xDAU, 0xFEU, 0xFFU, 0xFFU};
    std::memcpy(batch_out + 12, batch_suffix.data(), batch_suffix.size());
    auto* loop = batch_out - 273;
    loop[0] = 0x8BU;
    loop[1] = 0x15U;
    write_u32(loop + 2, available_end_address);
    loop[6] = 0xA1U;
    write_u32(loop + 7, cursor_address);
    loop[11] = 0x39U;
    loop[12] = 0xD0U;
    loop[13] = 0x7CU;
    loop[14] = 0x1BU;
    auto* service = loop + 42;
    const std::array<std::uint8_t, 12> service_prefix = {
        0x83U, 0xF9U, 0x64U, 0x7CU, 0x21U, 0x3DU,
        0x6CU, 0x07U, 0x00U, 0x00U, 0x7CU, 0x1AU};
    std::memcpy(service, service_prefix.data(), service_prefix.size());
    write_u32(synthetic_arena.data() + kCursorOffset, 11U);
    write_u32(synthetic_arena.data() + kAvailableEndOffset, 15U);
    write_u32(synthetic_arena.data() + kFrameTargetOffset, 5U);
    write_u32(synthetic_arena.data() + kFrameCountOffset, 1U);
    const std::array<std::uint8_t, 4> batch_payload = {
        0x11U, 0x22U, 0x33U, 0x44U};
    std::memcpy(synthetic_arena.data() + kSourceBufferOffset + 11U,
                batch_payload.data(), batch_payload.size());
    platform::win32::Piu10Mp3FrameBatchPlan batch_plan;
    std::uint32_t batch_ecx = 100U;
    const bool batch_plan_valid =
        platform::win32::BuildPiu10Mp3FrameBatchPlan(
            batch_context.get(), runtime_address(kBatchOutOffset), 10U,
            &batch_plan) &&
        batch_plan.bytes.size() == batch_payload.size() &&
        batch_plan.service_counter_limit == 100U &&
        batch_plan.service_cursor_threshold == 0x76CU &&
        std::equal(batch_plan.bytes.begin(), batch_plan.bytes.end(),
                   batch_payload.begin());

    constexpr std::uint32_t kVariantBatchOutOffset = 0x00045678U;
    auto* variant_out = synthetic_arena.data() + kVariantBatchOutOffset;
    variant_out[-35] = 0xA1U;
    write_u32(variant_out - 34, cursor_address);
    variant_out[-30] = 0x8DU;
    variant_out[-29] = 0x50U;
    variant_out[-28] = 0x01U;
    variant_out[-27] = 0x89U;
    variant_out[-26] = 0x15U;
    write_u32(variant_out - 25, cursor_address);
    variant_out[-21] = 0x8BU;
    variant_out[-20] = 0x15U;
    write_u32(variant_out - 19, frame_count_address);
    variant_out[-15] = 0x42U;
    variant_out[-14] = 0x8AU;
    variant_out[-13] = 0x80U;
    write_u32(variant_out - 12, source_buffer_address);
    variant_out[-8] = 0x89U;
    variant_out[-7] = 0x15U;
    write_u32(variant_out - 6, frame_count_address);
    variant_out[-2] = 0x89U;
    variant_out[-1] = 0xF2U;
    variant_out[0] = 0xEEU;
    variant_out[1] = 0xA1U;
    write_u32(variant_out + 2, frame_count_address);
    variant_out[6] = 0x8BU;
    variant_out[7] = 0x1DU;
    write_u32(variant_out + 8, frame_target_address);
    variant_out[12] = 0x41U;
    variant_out[13] = 0x39U;
    variant_out[14] = 0xD8U;
    variant_out[15] = 0x0FU;
    variant_out[16] = 0x85U;
    auto* variant_loop = variant_out - 284;
    const std::int32_t variant_loop_displacement =
        static_cast<std::int32_t>(variant_loop - (variant_out + 21));
    std::memcpy(variant_out + 17, &variant_loop_displacement,
                sizeof(variant_loop_displacement));
    variant_loop[0] = 0x8BU;
    variant_loop[1] = 0x15U;
    write_u32(variant_loop + 2, available_end_address);
    variant_loop[6] = 0xA1U;
    write_u32(variant_loop + 7, cursor_address);
    variant_loop[11] = 0x39U;
    variant_loop[12] = 0xD0U;
    variant_loop[13] = 0x7CU;
    variant_loop[14] = 0x1BU;
    std::memcpy(variant_loop + 42, service_prefix.data(),
                service_prefix.size());
    platform::win32::Piu10Mp3FrameBatchPlan variant_plan;
    const bool variant_plan_valid =
        platform::win32::BuildPiu10Mp3FrameBatchPlan(
            batch_context.get(), runtime_address(kVariantBatchOutOffset),
            10U, &variant_plan) &&
        variant_plan.bytes.size() == batch_payload.size() &&
        variant_plan.service_counter_limit == 100U &&
        variant_plan.service_cursor_threshold == 0x76CU &&
        std::equal(variant_plan.bytes.begin(), variant_plan.bytes.end(),
                   batch_payload.begin());

    write_u32(batch_out - 18, cursor_address + 4U);
    platform::win32::Piu10Mp3FrameBatchPlan rejected_plan;
    const bool relocation_independent_fail_closed =
        !platform::win32::BuildPiu10Mp3FrameBatchPlan(
            batch_context.get(), runtime_address(kBatchOutOffset), 10U,
            &rejected_plan);
    write_u32(batch_out - 18, cursor_address);
    const bool batch_commit_valid = batch_plan_valid &&
        platform::win32::CommitPiu10Mp3FrameBatch(
            batch_plan, 3U, &batch_ecx) &&
        batch_ecx == 103U &&
        *batch_plan.source_cursor == 14U &&
        *batch_plan.frame_byte_count == 4U;
    const bool frame_batch_valid = batch_plan_valid && variant_plan_valid &&
        batch_commit_valid && relocation_independent_fail_closed;

    write_u32(synthetic_arena.data() + kCursorOffset, 11U);
    write_u32(synthetic_arena.data() + kFrameCountOffset, 1U);
    batch_context->piu10_mp3_frame_batch_audit_enabled = true;
    std::uint32_t audit_ecx = 100U;
    platform::win32::TransferPiu10Mp3FrameTail(
        batch_context.get(), runtime_address(kBatchOutOffset), 0xAAU,
        &audit_ecx);
    for (std::size_t index = 0U; index < batch_payload.size(); ++index)
    {
        write_u32(synthetic_arena.data() + kCursorOffset,
                  static_cast<std::uint32_t>(12U + index));
        write_u32(synthetic_arena.data() + kFrameCountOffset,
                  static_cast<std::uint32_t>(2U + index));
        audit_ecx = static_cast<std::uint32_t>(101U + index);
        platform::win32::TransferPiu10Mp3FrameTail(
            batch_context.get(), runtime_address(kBatchOutOffset),
            batch_payload[index], &audit_ecx);
    }
    const bool frame_audit_valid =
        batch_context->piu10_mp3_frame_batch_audit_passed_frames == 1U &&
        batch_context->piu10_mp3_frame_batch_audit_mismatches == 0U &&
        !batch_context->piu10_mp3_frame_batch_audit_active;

    std::vector<std::uint8_t> audit_payload(9001U, 0U);
    for (std::size_t index = 0U; index < audit_payload.size(); ++index)
    {
        audit_payload[index] = static_cast<std::uint8_t>(
            (index * 37U + 11U) & 0xFFU);
    }
    std::vector<sound::StreamChunkDigest> contiguous_digests;
    std::vector<sound::StreamChunkDigest> segmented_digests;
    const auto collect_digest = [](
        const void* output, const sound::StreamChunkDigest& digest) {
        const_cast<std::vector<sound::StreamChunkDigest>*>(
            static_cast<const std::vector<sound::StreamChunkDigest>*>(output))
            ->push_back(digest);
    };
    sound::StreamChunkAudit contiguous_audit;
    contiguous_audit.Consume(
        audit_payload, &contiguous_digests, collect_digest);
    sound::StreamChunkAudit segmented_audit;
    std::size_t audit_offset = 0U;
    while (audit_offset < audit_payload.size())
    {
        const std::size_t count = std::min<std::size_t>(
            137U, audit_payload.size() - audit_offset);
        segmented_audit.Consume(
            std::span<const std::uint8_t>(
                audit_payload.data() + audit_offset, count),
            &segmented_digests, collect_digest);
        audit_offset += count;
    }
    const auto digests_equal = [](
        const sound::StreamChunkDigest& left,
        const sound::StreamChunkDigest& right) {
        return left.chunk_index == right.chunk_index &&
            left.end_offset == right.end_offset &&
            left.hash == right.hash && left.byte_count == right.byte_count;
    };
    const bool stream_chunk_audit_valid =
        contiguous_digests.size() == 2U &&
        contiguous_digests.size() == segmented_digests.size() &&
        std::equal(contiguous_digests.begin(), contiguous_digests.end(),
                   segmented_digests.begin(), digests_equal) &&
        digests_equal(contiguous_audit.partial_digest(),
                      segmented_audit.partial_digest());

    board.Reset();
    const std::array<std::uint8_t, 8> expected_cat_bits = {
        1U, 0U, 1U, 0U, 1U, 0U, 1U, 0U};
    bool cat_sequence_valid =
        board.Write16(0x02D4U, 0x0100U) &&
        board.Write16(0x02D6U, 0x0001U) &&
        board.Write16(0x02DAU, 0x0000U);
    for (std::uint8_t expected : expected_cat_bits)
    {
        cat_sequence_valid = cat_sequence_valid &&
            board.Write16(0x02D4U, 0x0100U) &&
            board.Write16(0x02D6U, 0x0001U) &&
            board.Write16(0x02DAU, 0x0000U) &&
            board.Write16(0x02DAU, 0x0010U) &&
            board.Write16(0x02D4U, 0x0080U) &&
            board.Write16(0x02D6U, 0x0000U) &&
            board.Read16(0x02DAU, &value) &&
            ((value >> 5U) & 1U) == expected;
    }

    const std::array<std::uint8_t, 8> pumpitpc_transform = {
        0xF0U, 0x1CU, 0xFEU, 0x03U, 0x81U, 0x40U, 0x38U, 0xF8U};
    const std::array<std::uint8_t, 10> pumpitpc_challenge = {
        0xD0U, 0xAAU, 0xC9U, 0xF8U, 0x96U,
        0x4CU, 0xD0U, 0xDEU, 0xB6U, 0x0BU};
    const std::array<std::uint8_t, 10> pumpitpc_response = {
        0x7DU, 0x77U, 0xFEU, 0xEAU, 0x8BU,
        0x7AU, 0x55U, 0x7DU, 0x5FU, 0x1EU};
    hle::Piu10IsaBoard vector_board;
    std::vector<std::uint8_t> vector_flash(
        hle::Piu10IsaBoard::kFlashBytes, 0xFFU);
    std::string vector_message;
    bool cat_vector_valid = vector_board.Initialize(
        std::move(vector_flash), pumpitpc_transform, &vector_message) &&
        vector_board.Write16(0x02D4U, 0x0100U) &&
        vector_board.Write16(0x02D6U, 0x0001U) &&
        vector_board.Write16(0x02DAU, 0x0030U);
    std::array<std::uint8_t, 11> raw_response = {};
    std::array<std::uint8_t, 10> actual_response = {};
    for (std::size_t byte = 0; byte < pumpitpc_challenge.size(); ++byte)
    {
        for (unsigned bit = 0; bit < 8U; ++bit)
        {
            const std::uint16_t data =
                ((pumpitpc_challenge[byte] >> bit) & 1U) != 0U
                    ? 0U : 0x0020U;
            cat_vector_valid = cat_vector_valid &&
                vector_board.Write16(0x02D4U, 0x0100U) &&
                vector_board.Write16(0x02D6U, 0x0001U) &&
                vector_board.Write16(0x02DAU, data) &&
                vector_board.Write16(0x02DAU,
                                     static_cast<std::uint16_t>(data | 0x10U)) &&
                vector_board.Write16(0x02D4U, 0x0080U) &&
                vector_board.Write16(0x02D6U, 0x0000U) &&
                vector_board.Read16(0x02DAU, &value);
            raw_response[byte] = static_cast<std::uint8_t>(
                (raw_response[byte] << 1U) | ((value >> 5U) & 1U));
        }
    }
    for (unsigned bit = 0; bit < 2U; ++bit)
    {
        cat_vector_valid = cat_vector_valid &&
            vector_board.Write16(0x02D4U, 0x0100U) &&
            vector_board.Write16(0x02D6U, 0x0001U) &&
            vector_board.Write16(0x02DAU, 0x0000U) &&
            vector_board.Write16(0x02DAU, 0x0010U) &&
            vector_board.Write16(0x02D4U, 0x0080U) &&
            vector_board.Write16(0x02D6U, 0x0000U) &&
            vector_board.Read16(0x02DAU, &value);
        raw_response[pumpitpc_challenge.size()] =
            static_cast<std::uint8_t>(
                (raw_response[pumpitpc_challenge.size()] << 1U) |
                ((value >> 5U) & 1U));
    }
    raw_response[pumpitpc_challenge.size()] = static_cast<std::uint8_t>(
        raw_response[pumpitpc_challenge.size()] << 6U);
    const auto reverse_bits = [](std::uint8_t input) {
        std::uint8_t output = 0U;
        for (unsigned bit = 0; bit < 8U; ++bit)
        {
            output = static_cast<std::uint8_t>(
                (output << 1U) | ((input >> bit) & 1U));
        }
        return output;
    };
    for (std::size_t byte = 0; byte < actual_response.size(); ++byte)
    {
        const std::uint8_t aligned = static_cast<std::uint8_t>(
            (raw_response[byte] << 2U) |
            ((raw_response[byte + 1U] >> 6U) & 0x03U));
        actual_response[byte] = reverse_bits(aligned);
    }
    cat_vector_valid = cat_vector_valid &&
        actual_response == pumpitpc_response;

    const bool valid = target_profiles_valid && jamma_target_profiles_valid &&
        mp3_latency_profile_valid && mp3_latency_bytes_valid &&
        mp3_snapshot_valid &&
        dac_control_valid && dac_gain_valid &&
        mpeg_parser_valid && status_valid && status_source_valid && flash_valid &&
        mp3_stream_valid && ring_valid && frame_batch_valid &&
        frame_audit_valid && stream_chunk_audit_valid &&
        cat_sequence_valid && cat_vector_valid;
    std::cout << "piu10_target_profiles="
              << (target_profiles_valid ? "true" : "false") << "\n";
    std::cout << "jamma_target_profiles="
              << (jamma_target_profiles_valid ? "true" : "false") << "\n";
    std::cout << "piu10_mp3_latency_profile="
              << (mp3_latency_profile_valid ? "true" : "false")
              << ",milliseconds="
              << (pumpito_profile != nullptr
                      ? pumpito_profile->piu10_mp3_latency_ms : 0U)
              << "\n";
    std::cout << "piu10_mp3_latency_bytes="
              << (mp3_latency_bytes_valid ? "true" : "false")
              << ",bytes="
              << platform::win32::Piu10Mp3AudioOut::
                     CalculateStartupSilenceBytes(50U, 44100, 2)
              << "\n";
    std::cout << "piu10_dac3350a_control="
              << (dac_control_valid ? "true" : "false")
              << ",events=" << dac_events.size() << "\n";
    std::cout << "piu10_mp3_snapshot="
              << (mp3_snapshot_valid ? "true" : "false")
              << ",queued=" << unopened_snapshot.pcm_queued_bytes
              << ",pending=" << unopened_snapshot.decoder_pending_bytes
              << "\n";
    std::cout << "piu10_dac3350a_gain="
              << (dac_gain_valid ? "true" : "false")
              << ",unity="
              << sound::Dac3350aControl::CalculateAnalogGain(44U) << "\n";
    std::cout << "piu10_isa_board_probe=" << (valid ? "true" : "false")
              << ",destination=0x" << std::hex << board.destination()
              << ",value=0x" << value << std::dec << "\n";
    std::cout << "piu10_cat702_vector="
              << (cat_vector_valid ? "true" : "false") << ",response=";
    for (const std::uint8_t byte : actual_response)
    {
        std::cout << std::hex << static_cast<unsigned>(byte) << ":";
    }
    std::cout << std::dec << "\n";
    std::cout << "piu10_mp3_ring=" << (ring_valid ? "true" : "false")
              << ",demand_low=" << (demand_deasserted ? "true" : "false")
              << ",pop_keeps_low="
              << (demand_stays_low_after_pop ? "true" : "false")
              << ",demand_high=" << (demand_reasserted ? "true" : "false")
              << ",inflight_high="
              << decoder_fifo.inflight_high_water() << "\n";
    std::cout << "piu10_mp3_frame_batch="
              << (frame_batch_valid ? "true" : "false")
              << ",bytes=" << batch_plan.bytes.size()
              << ",ecx=" << batch_ecx
              << ",relocated=true,variant="
              << (variant_plan_valid ? "true" : "false")
              << ",fail-closed="
              << (relocation_independent_fail_closed ? "true" : "false")
              << "\n";
    std::cout << "piu10_mp3_frame_audit="
              << (frame_audit_valid ? "true" : "false")
              << ",frames="
              << batch_context->piu10_mp3_frame_batch_audit_passed_frames
              << "\n";
    std::cout << "piu10_mp3_stream_chunk_audit="
              << (stream_chunk_audit_valid ? "true" : "false")
              << ",chunks=" << contiguous_digests.size() << "\n";
    return valid;
}

}  // namespace repiu::tools
