#include "repiu/platform/guest_stack_switch.h"

// Task 503d-16. The definitions behind the declarations in the header.
//
// They used to live in `execution_trampoline.cpp`, four of them with internal
// linkage, which worked while the only code reading them was MSVC inline
// assembly in that same translation unit. The GAS counterparts are a separate
// file and a separate object, so the storage has to be somewhere both can
// reach -- and this file is built on every host, which is what lets the Linux
// assembly link before the trampoline itself does.

extern "C" {

std::uint32_t g_recovery_host_fs = 0;
std::uint32_t g_recovery_host_ds = 0;
std::uint32_t g_recovery_host_es = 0;
std::uint32_t g_recovery_host_gs = 0;
std::uint32_t g_recovery_host_stack_base = 0;
std::uint32_t g_recovery_host_stack_limit = 0;

std::uint32_t g_repiu_dbt_host_esp = 0;
std::uint32_t g_repiu_dbt_host_stack_base = 0;
std::uint32_t g_repiu_dbt_host_stack_limit = 0;
std::uint32_t g_repiu_dbt_guest_stack_base = 0;
std::uint32_t g_repiu_dbt_guest_stack_limit = 0;

}  // extern "C"
