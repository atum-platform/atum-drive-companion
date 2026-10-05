// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "gui/opencloudguilib.h"

namespace OCC {
// Internal, parent-owned stdio service. Never constructs the desktop Application.
OPENCLOUD_GUI_EXPORT int runAtumEngine();
}
