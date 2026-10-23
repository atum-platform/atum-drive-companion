// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "gui/opencloudguilib.h"
#include "libsync/syncresult.h"

#include <QString>
#include <optional>

namespace OCC {
// Closed presentation mapping for the single enrolled folder. A completed run
// with ignored files is usable Drive state, not a stopped engine.
OPENCLOUD_GUI_EXPORT QString atumEngineFolderState(SyncResult::Status status);
OPENCLOUD_GUI_EXPORT bool atumEngineRunSucceeded(bool engineSuccess, SyncResult::Status status);

class OPENCLOUD_GUI_EXPORT AtumEngineCleanupCompletion
{
public:
    // Returns a terminal protocol state exactly once. A failed durable marker
    // is terminal even while it remains pending; an ordinary pending delete
    // must still wait for its asynchronous acknowledgement.
    std::optional<QString> finishCleanupIfSettled(bool pending, bool failed);

private:
    bool _finished = false;
};

// Internal, parent-owned stdio service. Never constructs the desktop Application.
OPENCLOUD_GUI_EXPORT int runAtumEngine();
}
