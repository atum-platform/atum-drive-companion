// SPDX-License-Identifier: GPL-2.0-or-later
#include "atumfilemetadata.h"

#include "folder.h"
#include "libsync/account.h"
#include "libsync/graphapi/space.h"
#include "libsync/graphapi/spacesmanager.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include <algorithm>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

namespace OCC {
namespace {
constexpr qint64 maxTaskFileBytes = 5 * 1024 * 1024;

bool printable(const QString &value, const int maximum)
{
    if (value.isEmpty() || value.size() > maximum) {
        return false;
    }
    for (const auto character : value) {
        if (character.unicode() < 0x21 || character.unicode() > 0x7e) {
            return false;
        }
    }
    return true;
}

bool requestIdValid(const QString &value)
{
    static const QRegularExpression expression(
        QStringLiteral("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"));
    return expression.match(value).hasMatch();
}

bool relativePathValid(const QString &value, const bool allowEmpty)
{
    if ((value.isEmpty() && !allowEmpty) || value.size() > 4096 || QDir::isAbsolutePath(value) || value.contains(QLatin1Char('\\'))) {
        return false;
    }
    if (value.isEmpty()) {
        return true;
    }
    const auto parts = value.split(QLatin1Char('/'));
    return std::all_of(parts.cbegin(), parts.cend(), [](const QString &part) { return !part.isEmpty() && part != QStringLiteral(".") && part != QStringLiteral(".."); });
}

AtumFileMetadataResult unavailable(const QString &requestId)
{
    return {AtumFileMetadataStatus::Unavailable,
        {{QStringLiteral("kind"), QStringLiteral("file_metadata")}, {QStringLiteral("requestId"), requestId}, {QStringLiteral("state"), QStringLiteral("unavailable")}}};
}

AtumFileMetadataResult malformed()
{
    return {AtumFileMetadataStatus::Malformed, {}};
}

bool authorityReady(Folder *folder, QString *storage, QString *space)
{
    if (!folder || folder->hasSetupError() || !folder->canSync() || folder->syncResult().status() != SyncResult::Success) {
        return false;
    }

    const auto accountState = folder->accountState();
    const auto account = accountState ? accountState->account() : AccountPtr{};
    if (!account || accountState->isSignedOut()) {
        return false;
    }

    const auto spaces = account->spacesManager()->spaces();
    QVector<GraphApi::Space *> personal;
    for (auto *candidate : spaces) {
        if (candidate && !candidate->disabled() && candidate->drive().getDriveType() == QStringLiteral("personal")) {
            personal.append(candidate);
        }
    }
    auto *folderSpace = folder->space();
    if (personal.size() != 1 || folderSpace != personal.first()) {
        return false;
    }

    const auto spaceId = folderSpace->drive().getRoot().getId();
    const auto separator = spaceId.indexOf(QLatin1Char('$'));
    if (separator <= 0 || separator != spaceId.lastIndexOf(QLatin1Char('$'))) {
        return false;
    }
    const auto storagePart = spaceId.left(separator);
    const auto spacePart = spaceId.mid(separator + 1);
    if (!printable(storagePart, 255) || !printable(spacePart, 255)) {
        return false;
    }
    *storage = storagePart;
    *space = spacePart;
    return true;
}

bool localRecordMatches(Folder *folder, const QString &relativePath, const SyncJournalFileRecord &record, const bool file)
{
#ifndef Q_OS_UNIX
    Q_UNUSED(folder)
    Q_UNUSED(relativePath)
    Q_UNUSED(record)
    Q_UNUSED(file)
    return false;
#else
    struct stat statBuffer {};
    const auto absolutePath = QDir(folder->path()).filePath(relativePath);
    if (::lstat(QFile::encodeName(absolutePath).constData(), &statBuffer) != 0 || S_ISLNK(statBuffer.st_mode)) {
        return false;
    }
    const bool localFile = S_ISREG(statBuffer.st_mode);
    const bool localDirectory = S_ISDIR(statBuffer.st_mode);
    if ((file && (!localFile || !record.isFile())) || (!file && (!localDirectory || !record.isDirectory()))) {
        return false;
    }
    if (record.inode() != static_cast<quint64>(statBuffer.st_ino) || record.modtime() != statBuffer.st_mtime) {
        return false;
    }
    return !file || record.size() == statBuffer.st_size;
#endif
}

AtumFileMetadataResult ready(const QString &requestId, const QString &storage, const QString &space, const QString &opaque, const QString &etag, const qint64 size,
    const bool includeVersion)
{
    QJsonObject message{{QStringLiteral("kind"), QStringLiteral("file_metadata")}, {QStringLiteral("requestId"), requestId}, {QStringLiteral("state"), QStringLiteral("ready")},
        {QStringLiteral("resource"), QJsonObject{{QStringLiteral("storage"), storage}, {QStringLiteral("space"), space}, {QStringLiteral("opaque"), opaque}}}};
    if (includeVersion) {
        message.insert(QStringLiteral("etag"), etag);
        message.insert(QStringLiteral("size"), size);
    }
    return {AtumFileMetadataStatus::Ready, message};
}

bool journalResourceOpaque(const QByteArray &fileId, const QString &storage, const QString &space, QString *opaque)
{
    const auto resourceId = QString::fromUtf8(fileId);
    const auto prefix = storage + QLatin1Char('$') + space + QLatin1Char('!');
    if (!resourceId.startsWith(prefix)) {
        return false;
    }
    const auto value = resourceId.mid(prefix.size());
    if (value.contains(QLatin1Char('!')) || !printable(value, 255)) {
        return false;
    }
    *opaque = value;
    return true;
}
}

AtumFileMetadataResult resolveAtumFileMetadata(Folder *folder, const QJsonObject &request)
{
    const auto requestId = request.value(QStringLiteral("requestId")).toString();
    const auto mode = request.value(QStringLiteral("mode")).toString();
    const auto relativePath = request.value(QStringLiteral("relativePath")).toString();
    if (request.size() != 4 || request.value(QStringLiteral("kind")).toString() != QStringLiteral("file_metadata") || !requestIdValid(requestId)
        || !relativePathValid(relativePath, mode == QStringLiteral("create"))
        || (mode != QStringLiteral("read") && mode != QStringLiteral("replace") && mode != QStringLiteral("create"))) {
        return malformed();
    }

    QString storage;
    QString space;
    if (!authorityReady(folder, &storage, &space)) {
        return unavailable(requestId);
    }
    // Graph's root id establishes storage$space only. It is not a CS3 opaque
    // resource id, and the journal intentionally has no record for root.
    if (relativePath.isEmpty()) {
        return unavailable(requestId);
    }

    const auto journal = folder->journalDb();
    if (!journal) {
        return unavailable(requestId);
    }
    const auto record = journal->getFileRecord(relativePath);
    if (!record.isValid() || record.hasDirtyPlaceholder() || !localRecordMatches(folder, relativePath, record, mode != QStringLiteral("create"))) {
        return unavailable(requestId);
    }

    QString opaque;
    if (!journalResourceOpaque(record.fileId(), storage, space, &opaque)) {
        return unavailable(requestId);
    }
    if (mode == QStringLiteral("create")) {
        return ready(requestId, storage, space, opaque, {}, 0, false);
    }

    const auto etag = record.etag();
    if (!printable(etag, 512) || record.size() < 0 || record.size() > maxTaskFileBytes) {
        return unavailable(requestId);
    }
    return ready(requestId, storage, space, opaque, etag, record.size(), true);
}
}
