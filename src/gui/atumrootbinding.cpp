// SPDX-License-Identifier: GPL-2.0-or-later
#include "atumrootbinding.h"
#include "common/utility.h"
#include "guiutility.h"
#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace Qt::Literals::StringLiterals;
namespace OCC {
namespace {
    const auto markerName = u".atum-drive-binding.json"_s;
    const auto ownerName = u".atum-drive-owner.lock"_s;

    bool sameFile(qintptr handle, const QString &path)
    {
#ifdef Q_OS_WIN
        BY_HANDLE_FILE_INFORMATION held{}, current{};
        const auto file = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return false;
        }
        const bool ok = GetFileInformationByHandle(reinterpret_cast<HANDLE>(handle), &held) && GetFileInformationByHandle(file, &current)
            && !(current.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && held.dwVolumeSerialNumber == current.dwVolumeSerialNumber
            && held.nFileIndexHigh == current.nFileIndexHigh && held.nFileIndexLow == current.nFileIndexLow;
        CloseHandle(file);
        return ok;
#else
        struct stat held{}, current{};
        const auto name = QFile::encodeName(path);
        return ::fstat(static_cast<int>(handle), &held) == 0 && ::lstat(name.constData(), &current) == 0 && !S_ISLNK(current.st_mode)
            && held.st_dev == current.st_dev && held.st_ino == current.st_ino;
#endif
    }


    Result<QString, QByteArray> safeRoot(const QString &path, bool mayCreate = false)
    {
        const QFileInfo info(path);
        const QFileInfo parent(info.path());
        const bool missing = !info.exists() && !info.isSymLink();
        if (!QDir::isAbsolutePath(path)
            || (missing ? (!mayCreate || !parent.isDir() || !parent.isWritable()) : (!info.isDir() || !info.isReadable() || !info.isWritable()))) {
            return QByteArray("Choose an existing readable and writable folder for Atum Drive.");
        }
        const auto root = QDir::cleanPath(missing ? QDir(parent.canonicalFilePath()).filePath(info.fileName()) : info.canonicalFilePath());
        const auto home = QDir::cleanPath(QFileInfo(QDir::homePath()).canonicalFilePath());
        if (root.isEmpty() || root == home || home.startsWith(root + u'/') || QDir(root).isRoot()) {
            return QByteArray("The home folder and its ancestors cannot be enrolled in Atum Drive.");
        }
        const QStringList managed = {u".ssh"_s, u".aws"_s, u".codex"_s, u".agents"_s, u".hermes"_s, u".atum"_s, u".atum-managed"_s, u".protected-skills"_s,
            u"node_modules"_s, u".venv"_s};
        for (const auto &component : root.split(u'/')) {
            if (managed.contains(component, Qt::CaseInsensitive)) {
                return QByteArray("Managed runtime and credential folders cannot be enrolled in Atum Drive.");
            }
        }
        for (auto parent = root;; parent = QFileInfo(parent).path()) {
            const auto tags = Utility::getDirectorySyncRootMarkings(parent);
            if (!tags.first.isEmpty() && (parent != root || tags.first != Theme::instance()->orgDomainName())) {
                return QByteArray("This folder overlaps another application's enrolled sync root.");
            }
            if (parent != root && QFileInfo::exists(QDir(parent).filePath(markerName))) {
                return QByteArray("This folder is inside another Atum Drive root.");
            }
            if (QDir(parent).isRoot()) {
                break;
            }
        }
        return root;
    }

    Result<QJsonObject, QString> readBinding(const QString &root)
    {
        const auto path = QDir(root).filePath(markerName);
        if (QFileInfo(path).isSymLink()) {
            return u"The root binding must not be a symbolic link."_s;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 8192) {
            return u"The root binding is unavailable. Files and journal have been preserved."_s;
        }
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            return u"The root binding is corrupt. Files and journal have been preserved."_s;
        }
        return document.object();
    }

    bool emptyForEnrollment(const QString &root, bool hasBinding)
    {
        for (const auto &entry : QDir(root).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot)) {
            if (entry == ownerName || entry == u".DS_Store"_s || (hasBinding && entry == markerName)) {
                continue;
            }
            return false;
        }
        return true;
    }

    QJsonObject expectedBinding(const QString &root, const AtumRootIdentity &identity)
    {
        return {{u"version"_s, 1}, {u"origin"_s, identity.origin}, {u"issuer"_s, identity.issuer}, {u"subject"_s, identity.subject},
            {u"space"_s, identity.space}, {u"canonicalRoot"_s, root}, {u"journal"_s, AtumRootBinding::journalName()}};
    }

    bool journalLinksSafe(const QString &root)
    {
        for (const auto &suffix : {QString(), u"-wal"_s, u"-shm"_s, u"-journal"_s, u".ctmp"_s}) {
            if (QFileInfo(QDir(root).filePath(AtumRootBinding::journalName() + suffix)).isSymLink()) {
                return false;
            }
        }
        return true;
    }
}

QString AtumRootBinding::journalName()
{
    return u".atum-drive-journal.db"_s;
}

Result<void, QString> AtumRootBinding::checkCandidate(const QString &path, const QString &issuer, const QString &subject)
{
    auto checked = safeRoot(path, true);
    if (!checked) {
        return QString::fromUtf8(checked.error());
    }
    const auto &root = *checked;
    if (QFileInfo::exists(QDir(root).filePath(markerName)) || QFileInfo(QDir(root).filePath(markerName)).isSymLink()) {
        auto binding = readBinding(root);
        if (!binding || binding->value(u"issuer"_s).toString() != issuer || binding->value(u"subject"_s).toString() != subject
            || binding->value(u"canonicalRoot"_s).toString() != root) {
            return u"This root is bound to a different identity or location. Choose another root; existing files will be preserved."_s;
        }
    } else if (!emptyForEnrollment(root, false)) {
        return u"This folder already contains files. Choose an empty folder; inventory approval is required before adopting existing files."_s;
    }
    return {};
}

AtumRootBinding::AtumRootBinding(const QString &root, const QJsonObject &binding)
    : _root(root)
    , _binding(binding)
{
}

bool AtumRootBinding::lockOwner()
{
    const auto ownerPath = QDir(_root).filePath(ownerName);
#ifdef Q_OS_WIN
    const auto directory = CreateFileW(reinterpret_cast<LPCWSTR>(_root.utf16()), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (directory == INVALID_HANDLE_VALUE) {
        return false;
    }
    _directoryHandle = reinterpret_cast<qintptr>(directory);
    const auto owner = CreateFileW(reinterpret_cast<LPCWSTR>(ownerPath.utf16()), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_ALWAYS, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (owner == INVALID_HANDLE_VALUE) {
        return false;
    }
    _ownerHandle = reinterpret_cast<qintptr>(owner);
    OVERLAPPED overlapped{};
    if (!LockFileEx(owner, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, MAXDWORD, MAXDWORD, &overlapped)) {
        return false;
    }
#else
    _directoryHandle = ::open(QFile::encodeName(_root).constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (_directoryHandle == -1) {
        return false;
    }
    _ownerHandle = ::openat(static_cast<int>(_directoryHandle), QFile::encodeName(ownerName).constData(), O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (_ownerHandle == -1 || ::flock(static_cast<int>(_ownerHandle), LOCK_EX | LOCK_NB) != 0) {
        return false;
    }
#endif
    return sameFile(_directoryHandle, _root) && sameFile(_ownerHandle, ownerPath);
}

AtumRootBinding::~AtumRootBinding()
{
    // Never unlink by pathname: a disconnected/replaced root can belong to a new owner.
    // Closing our handle releases only our inode's OS lock, including after a crash.
#ifdef Q_OS_WIN
    if (_ownerHandle != -1) {
        CloseHandle(reinterpret_cast<HANDLE>(_ownerHandle));
    }
    if (_directoryHandle != -1) {
        CloseHandle(reinterpret_cast<HANDLE>(_directoryHandle));
    }
#else
    if (_ownerHandle != -1) {
        ::close(static_cast<int>(_ownerHandle));
    }
    if (_directoryHandle != -1) {
        ::close(static_cast<int>(_directoryHandle));
    }
#endif
}

Result<std::unique_ptr<AtumRootBinding>, QString> AtumRootBinding::acquire(const QString &path, const AtumRootIdentity &identity, bool enrollEmpty)
{
    if (identity.origin.isEmpty() || identity.issuer.isEmpty() || identity.subject.isEmpty() || identity.space.isEmpty()) {
        return u"Sign in again to confirm this root's identity before syncing."_s;
    }
    auto checked = safeRoot(path);
    if (!checked) {
        return QString::fromUtf8(checked.error());
    }
    const auto &root = *checked;
    if (QFileInfo(QDir(root).filePath(ownerName)).isSymLink()) {
        return u"The root owner file must not be a symbolic link."_s;
    }
    auto owner = std::unique_ptr<AtumRootBinding>(new AtumRootBinding(root, expectedBinding(root, identity)));
    // Reject known collisions before creating an owner file. Repeat these checks under ownership below.
    const auto marker = QDir(root).filePath(markerName);
    const bool hasMarker = QFileInfo::exists(marker) || QFileInfo(marker).isSymLink();
    if (hasMarker) {
        auto binding = readBinding(root);
        if (!binding || *binding != owner->_binding) {
            return u"The root binding does not match. Existing files and journal have been preserved."_s;
        }
    } else if (!enrollEmpty || !emptyForEnrollment(root, false)) {
        return u"This root is not enrolled. Existing files and journals have been preserved."_s;
    }
    if (!owner->lockOwner()) {
        return u"This root is unavailable or owned by another Atum Drive process. Sync is paused; files and journal have been preserved."_s;
    }
    if (!journalLinksSafe(root)) {
        return u"The journal must not be a symbolic link. Files have been preserved."_s;
    }
    if (QFileInfo::exists(marker) || QFileInfo(marker).isSymLink()) {
        auto binding = readBinding(root);
        if (!binding || *binding != owner->_binding) {
            return u"The root's identity, space, location or journal does not match. Existing files and journal have been preserved."_s;
        }
        // Permit recovery between atomic binding creation and the first journal open only while the root is empty.
        if (!QFileInfo::exists(QDir(root).filePath(journalName())) && !emptyForEnrollment(root, true)) {
            return u"The bound root has files but no journal. Sync is paused pending inventory recovery."_s;
        }
    } else {
        if (!enrollEmpty || !emptyForEnrollment(root, false)) {
            return u"This root is not enrolled. Existing files and journals have been preserved."_s;
        }
        QSaveFile file(marker);
        const auto bytes = QJsonDocument(owner->_binding).toJson(QJsonDocument::Compact);
        if (!sameFile(owner->_directoryHandle, root) || !file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
            return u"Could not save the root binding. Sync has not started."_s;
        }
    }
    return std::move(owner);
}

bool AtumRootBinding::matches(const AtumRootIdentity &identity) const
{
    if (_ownerHandle == -1 || _directoryHandle == -1 || !sameFile(_directoryHandle, _root) || !sameFile(_ownerHandle, QDir(_root).filePath(ownerName))
        || !journalLinksSafe(_root) || expectedBinding(_root, identity) != _binding) {
        return false;
    }
    auto binding = readBinding(_root);
    return binding && *binding == _binding;
}

QString AtumRootBinding::canonicalRoot() const
{
    return _root;
}
}
