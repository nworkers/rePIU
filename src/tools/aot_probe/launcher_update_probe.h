#pragma once

namespace repiu::tools
{

// Issue #48: the launcher update path without a network -- versions, SHA-256,
// the release response, both archive formats, the install with its rollback,
// and the updater's states through a fake fetch.
bool RunLauncherUpdateProbe();

}  // namespace repiu::tools
