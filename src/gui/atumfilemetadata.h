// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "gui/opencloudguilib.h"

#include <QJsonObject>

namespace OCC {
class Folder;

enum class AtumFileMetadataStatus {
    Malformed,
    Unavailable,
    Ready,
};

struct OPENCLOUD_GUI_EXPORT AtumFileMetadataResult {
    AtumFileMetadataStatus status;
    QJsonObject message;
};

/** Resolve one already-contained local path to current remote authority facts.
 *
 * This is a private stdio-helper seam. The caller owns the renderer's opaque
 * row handle and supplies only its validated path relative to the sync root.
 */
OPENCLOUD_GUI_EXPORT AtumFileMetadataResult resolveAtumFileMetadata(Folder *folder, const QJsonObject &request);
}
