#ifndef REPIU_CONFIG_ROMSET_CONFIG_TEMPLATE_H_
#define REPIU_CONFIG_ROMSET_CONFIG_TEMPLATE_H_

#include "repiu/input/jamma_input_bindings.h"

#include <string>
#include <string_view>

namespace repiu::config
{

// Renders the file rePIU writes when a ROM set has no config file yet.
//
// Every `[Input]` entry comes out commented, carrying the value currently in
// effect. Writing them active would defeat the parent-ROM-set layering: the
// generated file names all fourteen inputs, so after one run the child file
// would override everything and editing a shared cfg/pumpitup.ini could never
// have an effect again. Commented entries also make "generating the file does
// not change behavior" true by construction rather than by argument.
//
// The `[Input]` section header itself stays active so that an entry the user
// uncomments still belongs to a section.
//
// Pure string generation with no filesystem access, so the probe can check the
// result without writing anything.
std::string RenderRomSetConfigTemplate(
    std::string_view rom_set_id,
    const input::ResolvedJammaBindings& bindings);

// The line terminator the generated file uses. CRLF because the file is meant
// to be opened in a stock Windows editor.
constexpr std::string_view kRomSetConfigLineEnding = "\r\n";

}  // namespace repiu::config

#endif  // REPIU_CONFIG_ROMSET_CONFIG_TEMPLATE_H_
