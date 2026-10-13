// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "gui/opencloudguilib.h"
#include "libsync/syncresult.h"

#include <QString>

namespace OCC {
// Closed presentation mapping for the single enrolled folder. A completed run
// with ignored files is usable Drive state, not a stopped engine.
OPENCLOUD_GUI_EXPORT QString atumEngineFolderState(SyncResult::Status status);
OPENCLOUD_GUI_EXPORT bool atumEngineRunSucceeded(bool engineSuccess, SyncResult::Status status);

// Internal, parent-owned stdio service. Never constructs the desktop Application.
OPENCLOUD_GUI_EXPORT int runAtumEngine();
}
