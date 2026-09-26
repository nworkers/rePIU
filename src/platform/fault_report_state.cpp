#include "repiu/platform/fault_handler.h"

namespace repiu::platform
{

// Task 743. Written by the engine at every VEH exit, read by the unhandled
// fault report, so a refused exception names the decision that refused it.
volatile std::uint32_t repiu_last_veh_exit_site = 0U;
volatile std::uint32_t repiu_last_veh_exit_eip = 0U;

}  // namespace repiu::platform
