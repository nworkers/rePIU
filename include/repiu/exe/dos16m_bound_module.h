#ifndef REPIU_EXE_DOS16M_BOUND_MODULE_H_
#define REPIU_EXE_DOS16M_BOUND_MODULE_H_

#include "repiu/exe/executable_headers.h"

#include <cstdint>
#include <string>
#include <vector>

namespace repiu::exe
{

struct Dos16mBoundSegment
{
    std::uint16_t selector = 0;
    std::uint16_t limit = 0;
    std::uint8_t access = 0;
    std::uint16_t memory_paragraphs = 0;
    std::vector<std::uint8_t> image;
    std::vector<std::uint16_t> selector_relocation_offsets;
};

struct Dos16mBoundModule
{
    std::string name;
    std::uint32_t header_file_offset = 0;
    std::uint32_t next_header_file_offset = 0;
    std::uint16_t initial_cs = 0;
    std::uint16_t initial_ip = 0;
    std::uint32_t relocation_count = 0;
    std::vector<Dos16mBoundSegment> segments;
};

bool ParseDos16mBoundModules(const std::vector<std::uint8_t>& data,
                             std::vector<Dos16mBoundModule>* modules,
                             ParseError* error);

const Dos16mBoundModule* FindDos16mBoundModule(
    const std::vector<Dos16mBoundModule>& modules,
    const std::string& name);

}  // namespace repiu::exe

#endif  // REPIU_EXE_DOS16M_BOUND_MODULE_H_
