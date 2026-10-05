// SPDX-License-Identifier: GPL-2.0-or-later
#include "configfile.h"
#include "gui/accountmanager.h"
#include "gui/accountstate.h"
#include "gui/atumfilemetadata.h"
#include "gui/atumrootbinding.h"
#include "gui/folderman.h"
#include "gui/newwizard/pages/accountconfiguredwizardpage.h"
#include "libsync/accessmanager.h"
#include "libsync/graphapi/spacesmanager.h"
#include "testutils/syncenginetestutils.h"
#include "theme.h"
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QSignalSpy>
#include <future>
#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

using namespace OCC;
using namespace Qt::Literals::StringLiterals;

// Credentials/remote HTTP are synthetic; Folder, journal, locks, settings, VFS-off and removal are real.
class RootGraphManager : public OCC::AccessManager
{
public:
    explicit RootGraphManager(std::shared_ptr<QJsonArray> drives)
        : _drives(std::move(drives))
    {
    }

private:
    std::shared_ptr<QJsonArray> _drives;

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *) override
    {
        if (request.url().path() == u"/graph/v1.0/me/drives") {
            return new FakePayloadReply(op, request, QJsonDocument(QJsonObject{{u"value"_s, *_drives}}).toJson(), {}, this);
        }
        QJsonObject data;
        if (request.url().path() == u"/status.php") {
            data = {{u"installed"_s, true}, {u"maintenance"_s, false}, {u"needsDbUpgrade"_s, false}, {u"version"_s, u"10.11.0.0"_s},
                {u"product"_s, u"OpenCloud"_s}, {u"productversion"_s, u"7.2.4"_s}};
        } else if (request.url().path() == u"/graph/v1.0/me") {
            data = {{u"id"_s, u"synthetic-root-owner"_s}, {u"displayName"_s, u"Disposable"_s}};
        } else if (request.url().path() == u"/ocs/v2.php/cloud/capabilities") {
            data = {{u"ocs"_s,
                QJsonObject{{u"meta"_s, QJsonObject{{u"statuscode"_s, 100}}},
                    {u"data"_s, QJsonObject{{u"capabilities"_s, QJsonObject::fromVariantMap(TestUtils::testCapabilities())}}}}}};
        } else {
            return new FakeErrorReply(op, request, this, 404);
        }
        return new FakePayloadReply(op, request, QJsonDocument(data).toJson(), {}, this);
    }
};

class RootCredentials : public FakeCredentials
{
public:
    explicit RootCredentials(std::shared_ptr<QJsonArray> drives)
        : FakeCredentials(nullptr)
        , _drives(std::move(drives))
    {
        _wasFetched = true;
    }
    OCC::AccessManager *createAM() const override { return new RootGraphManager(_drives); }

private:
    std::shared_ptr<QJsonArray> _drives;
};

class TestAtumRootFolder : public QObject
{
    Q_OBJECT
    AccountStatePtr _state;
    std::shared_ptr<QJsonArray> _drives;

    AccountStatePtr account(const QString &root, const QString &subject = u"synthetic-root-owner"_s)
    {
        const auto profile = Theme::instance()->oauthIdentityProfile();
        auto acc = Account::create(QUuid::createUuid());
        acc->setUrl(profile->driveOrigin);
        acc->setAtumIdentity(profile->issuer, subject);
        acc->setDefaultSyncRoot(root);
        _drives = std::make_shared<QJsonArray>();
        acc->setCredentials(new RootCredentials(_drives));
        acc->setCapabilities({acc->url(), TestUtils::testCapabilities()});
        _state = AccountManager::instance()->addAccount(acc);
        return _state;
    }

    FolderDefinition definition(const QString &root)
    {
        const auto url = Utility::concatUrlPath(_state->account()->url(), u"/remote.php/dav/spaces/personal-root"_s);
        FolderDefinition def(_state->account()->uuid(), url, u"personal-root"_s, u"Personal"_s);
        def.setLocalPath(root);
        return def;
    }

    QJsonObject drive(const QString &id)
    {
        return {{u"id"_s, id}, {u"name"_s, u"Personal"_s}, {u"driveType"_s, u"personal"_s},
            {u"root"_s, QJsonObject{{u"id"_s, id}, {u"webDavUrl"_s, definition(QString()).webDavUrl().toString()}}}};
    }

    void refreshGraph()
    {
        QSignalSpy updated(_state->account()->spacesManager(), &GraphApi::SpacesManager::updated);
        Q_EMIT _state->account()->credentialsFetched();
        QTRY_VERIFY_WITH_TIMEOUT(!updated.isEmpty(), 3000);
    }

    int savedCount()
    {
        auto settings = ConfigFile::makeQSettings();
        const auto count = settings.beginReadArray(u"Folders");
        settings.endArray();
        return count;
    }

    void closeFolders()
    {
        TestUtils::folderMan()->unloadAndDeleteAllFolders();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

private Q_SLOTS:
    void testRootChooserRemainsVisible()
    {
        const auto suggested = QDir::home().filePath(u"Atum"_s);
        Wizard::AccountConfiguredWizardPage page(suggested, suggested);
        auto *directory = page.findChild<QLineEdit *>(u"localDirectoryLineEdit"_s);
        auto *advanced = page.findChild<QGroupBox *>(u"advancedConfigGroupBox"_s);
        auto *syncMode = page.findChild<QGroupBox *>(u"syncModeGroupBox"_s);
        QVERIFY(directory);
        QVERIFY(advanced);
        QVERIFY(syncMode);
        QVERIFY(directory->isVisibleTo(&page));
        QVERIFY(directory->isEnabled());
        QVERIFY(!advanced->isCheckable());
        QVERIFY(!syncMode->isVisibleTo(&page));
        page.setShowAdvancedSettings(false);
        QVERIFY(directory->isVisibleTo(&page));
        const auto chosen = QDir::temp().filePath(u"explicit-atum-root"_s);
        directory->setText(chosen);
        QCOMPARE(page.syncTargetDir(), QDir::toNativeSeparators(chosen));
        QCOMPARE(static_cast<int>(page.syncMode()), static_cast<int>(Wizard::SyncMode::SyncEverything));
    }

    void initTestCase()
    {
        if (!Theme::instance()->oauthIdentityProfile()) {
            QSKIP("Requires a compiled Atum OEM profile; unbranded regression CI is not Atum lifecycle evidence.");
        }
        TestUtils::folderMan()->setSyncEnabled(false);
    }

    void cleanup()
    {
        closeFolders();
        if (_state) {
            AccountManager::instance()->deleteAccount(_state);
            _state.clear();
        }
        auto settings = ConfigFile::makeQSettings();
        settings.remove(u"Folders");
        settings.sync();
        TestUtils::folderMan()->loadFolders();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void enrollResumeSignoutAndRemovalPreserveJournal()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        account(root.path());
        auto *man = TestUtils::folderMan();
        auto def = definition(root.path());
        def.virtualFilesMode = Vfs::Mode::WindowsCfApi; // Forced Off must choose the plugin from the effective definition.
        auto *folder = man->addFolder(_state, def);
        QVERIFY(folder);
        QVERIFY(!folder->hasSetupError());
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, {u"pending-upload/"_s});
        folder->journalDb()->commit(u"root lifecycle proof"_s);
        const auto filePath = QDir(root.path()).filePath(u"pending-upload.txt"_s);
        QVERIFY(TestUtils::writeRandomFile(filePath, 128));
        const auto journal = QDir(root.path()).filePath(AtumRootBinding::journalName());
        QVERIFY(QFileInfo::exists(journal));
        _state->signOutByUi();
        QCOMPARE(man->folders().size(), 1);
        QCOMPARE(savedCount(), 1);
        QVERIFY(!folder->canSync());
        closeFolders();
        QCOMPARE(man->loadFolders().value(), 1);
        folder = man->folders().first();
        QVERIFY(!folder->hasSetupError());
        bool ok = false;
        QCOMPARE(folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, &ok), QSet<QString>{u"pending-upload/"_s});
        QVERIFY(ok);
        QVERIFY(QFileInfo::exists(filePath));
        man->removeFolder(folder);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(savedCount(), 0); // Explicit removal forgets the account attachment, never local data.
        QVERIFY(QFileInfo::exists(journal));
        QVERIFY(QFileInfo::exists(QDir(root.path()).filePath(u".atum-drive-binding.json"_s)));
        QVERIFY(QFileInfo::exists(filePath));
        folder = man->addFolder(_state, definition(root.path()));
        QVERIFY(folder);
        QVERIFY(!folder->hasSetupError());
        QCOMPARE(folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, &ok), QSet<QString>{u"pending-upload/"_s});
        QVERIFY(ok);
    }

    void fileMetadataResolvesOnlyCurrentJournalRecords()
    {
#ifndef Q_OS_UNIX
        QSKIP("The private helper is Unix-only, like the Atum engine.");
#else
        QTemporaryDir root;
        QVERIFY(root.isValid());
        account(root.path());
        auto def = definition(root.path());
        def.setSpaceId(u"storage$personal"_s);
        auto *folder = TestUtils::folderMan()->addFolder(_state, def);
        QVERIFY(folder && !folder->hasSetupError());
        *_drives = {drive(u"storage$personal"_s)};
        refreshGraph();
        QTRY_VERIFY_WITH_TIMEOUT(folder->canSync(), 3000);
        folder->setSyncPaused(true);
        folder->setSyncPaused(false);
        QCOMPARE(folder->syncResult().status(), SyncResult::Success);

        const auto request = [](const QString &mode, const QString &relativePath) {
            return QJsonObject{{u"kind"_s, u"file_metadata"_s}, {u"requestId"_s, u"1a2b3c4d-5e6f-4a7b-8c9d-0e1f2a3b4c5d"_s},
                {u"mode"_s, mode}, {u"relativePath"_s, relativePath}};
        };
        const auto store = [&](const QString &relativePath, const ItemType type, const QByteArray &fileId, const QString &etag, const bool dirty = false) {
            struct stat local {};
            const auto path = QDir(root.path()).filePath(relativePath);
            QCOMPARE(::lstat(QFile::encodeName(path).constData(), &local), 0);
            auto item = TestUtils::dummyItem(relativePath);
            item._type = type;
            item._fileId = fileId;
            item._etag = etag;
            item._size = local.st_size;
            item._inode = static_cast<quint64>(local.st_ino);
            item._modtime = local.st_mtime;
            auto record = SyncJournalFileRecord::fromSyncFileItem(item);
            record.setDirtyPlaceholder(dirty);
            QVERIFY(folder->journalDb()->setFileRecord(record));
        };

        const auto input = QDir(root.path()).filePath(u"input.txt"_s);
        QVERIFY(TestUtils::writeRandomFile(input, 128));
        store(u"input.txt"_s, ItemTypeFile, "storage$personal!input-id", u"input-etag"_s);
        QVERIFY(QDir(root.path()).mkpath(u"outputs"_s));
        store(u"outputs"_s, ItemTypeDirectory, "storage$personal!outputs-id", u"outputs-etag"_s);

        for (const auto &mode : {u"read"_s, u"replace"_s}) {
            const auto result = resolveAtumFileMetadata(folder, request(mode, u"input.txt"_s));
            QVERIFY(result.status == AtumFileMetadataStatus::Ready);
            QCOMPARE(result.message.value(u"state"_s).toString(), u"ready"_s);
            QCOMPARE(result.message.value(u"resource"_s).toObject(), (QJsonObject{{u"storage"_s, u"storage"_s}, {u"space"_s, u"personal"_s}, {u"opaque"_s, u"input-id"_s}}));
            QCOMPARE(result.message.value(u"etag"_s).toString(), u"input-etag"_s);
            QCOMPARE(result.message.value(u"size"_s).toInteger(), 128);
        }

        const auto create = resolveAtumFileMetadata(folder, request(u"create"_s, u"outputs"_s));
        QVERIFY(create.status == AtumFileMetadataStatus::Ready);
        QCOMPARE(create.message.value(u"resource"_s).toObject(), (QJsonObject{{u"storage"_s, u"storage"_s}, {u"space"_s, u"personal"_s}, {u"opaque"_s, u"outputs-id"_s}}));
        QVERIFY(!create.message.contains(u"etag"_s));
        QVERIFY(!create.message.contains(u"size"_s));

        const auto rootCreate = resolveAtumFileMetadata(folder, request(u"create"_s, {}));
        QVERIFY(rootCreate.status == AtumFileMetadataStatus::Unavailable);
        const auto missing = resolveAtumFileMetadata(folder, request(u"read"_s, u"missing.txt"_s));
        QVERIFY(missing.status == AtumFileMetadataStatus::Unavailable);
        store(u"input.txt"_s, ItemTypeFile, "storage$personal!input-id", u"input-etag"_s, true);
        const auto dirty = resolveAtumFileMetadata(folder, request(u"read"_s, u"input.txt"_s));
        QVERIFY(dirty.status == AtumFileMetadataStatus::Unavailable);
        store(u"input.txt"_s, ItemTypeFile, "input-id", u"input-etag"_s);
        const auto bare = resolveAtumFileMetadata(folder, request(u"read"_s, u"input.txt"_s));
        QVERIFY(bare.status == AtumFileMetadataStatus::Unavailable);
        store(u"input.txt"_s, ItemTypeFile, "foreign$personal!input-id", u"input-etag"_s);
        const auto foreign = resolveAtumFileMetadata(folder, request(u"read"_s, u"input.txt"_s));
        QVERIFY(foreign.status == AtumFileMetadataStatus::Unavailable);
        store(u"input.txt"_s, ItemTypeFile, "storage$personalinput-id", u"input-etag"_s);
        const auto missingSeparator = resolveAtumFileMetadata(folder, request(u"read"_s, u"input.txt"_s));
        QVERIFY(missingSeparator.status == AtumFileMetadataStatus::Unavailable);
        store(u"input.txt"_s, ItemTypeFile, "storage$personal!input!again", u"input-etag"_s);
        const auto repeatedSeparator = resolveAtumFileMetadata(folder, request(u"read"_s, u"input.txt"_s));
        QVERIFY(repeatedSeparator.status == AtumFileMetadataStatus::Unavailable);
        store(u"input.txt"_s, ItemTypeFile, "storage$personal!bad\nopaque", u"input-etag"_s);
        const auto malformedOpaque = resolveAtumFileMetadata(folder, request(u"read"_s, u"input.txt"_s));
        QVERIFY(malformedOpaque.status == AtumFileMetadataStatus::Unavailable);
        store(u"input.txt"_s, ItemTypeFile, "storage$personal!input-id", u"bad\netag"_s);
        const auto malformedEtag = resolveAtumFileMetadata(folder, request(u"read"_s, u"input.txt"_s));
        QVERIFY(malformedEtag.status == AtumFileMetadataStatus::Unavailable);
        store(u"input.txt"_s, ItemTypeFile, "storage$personal!input-id", u"input-etag"_s);
        QVERIFY(TestUtils::writeRandomFile(input, 129));
        const auto stale = resolveAtumFileMetadata(folder, request(u"read"_s, u"input.txt"_s));
        QVERIFY(stale.status == AtumFileMetadataStatus::Unavailable);
        const auto malformed = resolveAtumFileMetadata(folder, request(u"read"_s, u"../input.txt"_s));
        QVERIFY(malformed.status == AtumFileMetadataStatus::Malformed);
#endif
    }

    void duplicateAliasParentAndChildNeverPersist()
    {
        QTemporaryDir parent;
        const auto root = QDir(parent.path()).filePath(u"Atum"_s);
        QVERIFY(QDir().mkpath(root));
        account(root);
        auto *man = TestUtils::folderMan();
        QVERIFY(man->addFolder(_state, definition(root)));
        QVERIFY(!man->addFolder(_state, definition(root)));
        QVERIFY(!man->addFolder(_state, definition(parent.path())));
        const auto child = QDir(root).filePath(u"child"_s);
        QVERIFY(QDir().mkpath(child));
        QVERIFY(!man->addFolder(_state, definition(child)));
#ifndef Q_OS_WIN
        const auto alias = QDir(parent.path()).filePath(u"alias"_s);
        QVERIFY(QFile::link(root, alias));
        QVERIFY(!man->addFolder(_state, definition(alias)));
#endif
        QCOMPARE(man->folders().size(), 1);
        QCOMPARE(savedCount(), 1);
    }

    void busyAndForeignResumePreserveSettingsAndBytes()
    {
        QTemporaryDir root;
        account(root.path());
        auto *man = TestUtils::folderMan();
        auto *folder = man->addFolder(_state, definition(root.path()));
        QVERIFY(folder && !folder->hasSetupError());
        closeFolders();
        auto held = AtumRootBinding::acquire(
            root.path(), {_state->account()->url().toString(), _state->account()->atumIssuer(), _state->account()->atumSubject(), u"personal-root"_s}, false);
        QVERIFY(held);
        auto heldOwner = *std::move(held);
        QFile journal(QDir(root.path()).filePath(AtumRootBinding::journalName()));
        QVERIFY(journal.open(QIODevice::ReadOnly));
        const auto before = journal.readAll();
        journal.close();
        QCOMPARE(man->loadFolders().value(), 1);
        QVERIFY(man->folders().first()->hasSetupError());
        QCOMPARE(savedCount(), 1);
        closeFolders();
        heldOwner.reset();
        _state->account()->setAtumIdentity(_state->account()->atumIssuer(), u"foreign-subject"_s);
        QCOMPARE(man->loadFolders().value(), 1);
        QVERIFY(man->folders().first()->hasSetupError());
        QVERIFY(journal.open(QIODevice::ReadOnly));
        QCOMPARE(journal.readAll(), before);
        QCOMPARE(savedCount(), 1);
    }

    void secondPersonalSpaceStopsResume()
    {
        QTemporaryDir root;
        account(root.path());
        auto *folder = TestUtils::folderMan()->addFolder(_state, definition(root.path()));
        QVERIFY(folder && !folder->hasSetupError());
        QTRY_VERIFY(folder->isReady());
        *_drives = {drive(u"personal-root"_s)};
        refreshGraph();
        // Use the real AccountState result slot to establish the normal connected preconditions.
        QVERIFY(QMetaObject::invokeMethod(_state.get(), "slotConnectionValidatorResult", Qt::DirectConnection,
            Q_ARG(ConnectionValidator::Status, ConnectionValidator::Connected), Q_ARG(QStringList, QStringList{})));
        QTRY_VERIFY_WITH_TIMEOUT(folder->canSync(), 3000);
        const auto rootCheck = folder->syncEngine().syncOptions()._localRootValid;
        _drives->append(drive(u"second-personal"_s));
        refreshGraph();
        QVERIFY(!folder->canSync());
        QCOMPARE(folder->syncState(), SyncResult::Paused);
        QVERIFY(std::async(std::launch::async, rootCheck).get()); // Worker guard reads files only; GUI readiness already stopped sync.
        *_drives = {drive(u"personal-root"_s)};
        refreshGraph();
        QTRY_VERIFY_WITH_TIMEOUT(folder->canSync(), 3000);
    }

    void orphanSettingsRemainInertAndPersisted()
    {
        QTemporaryDir root;
        account(root.path());
        auto def = definition(root.path());
        auto settings = ConfigFile::makeQSettings();
        settings.beginWriteArray(u"Folders", 1);
        settings.setArrayIndex(0);
        FolderDefinition orphan(QUuid::createUuid(), def.webDavUrl(), def.spaceId(), u"Unavailable"_s);
        orphan.setLocalPath(root.path());
        orphan.journalPath = AtumRootBinding::journalName();
        FolderDefinition::save(settings, orphan);
        settings.endArray();
        settings.sync();
        QCOMPARE(TestUtils::folderMan()->loadFolders().value(), 0);
        QVERIFY(!TestUtils::folderMan()->addFolder(_state, definition(root.path())));
        QCOMPARE(savedCount(), 1);
        QTemporaryDir healthy;
        _state->account()->setDefaultSyncRoot(healthy.path());
        auto *folder = TestUtils::folderMan()->addFolder(_state, definition(healthy.path()));
        QVERIFY(folder && !folder->hasSetupError());
        QCOMPARE(savedCount(), 2);
        TestUtils::folderMan()->removeFolder(folder);
        QCOMPARE(savedCount(), 1);
        QVERIFY(QDir(root.path()).entryList(QDir::Files | QDir::Hidden).isEmpty());
    }

    void replacedRootStopsEngineBeforeOpeningReplacementJournal()
    {
#ifdef Q_OS_WIN
        QSKIP("Windows retained directory handle deliberately prevents live root replacement.");
#else
        QTemporaryDir parent;
        const auto root = QDir(parent.path()).filePath(u"Atum"_s);
        const auto offline = root + u".offline";
        QVERIFY(QDir().mkpath(root));
        account(root);
        auto *folder = TestUtils::folderMan()->addFolder(_state, definition(root));
        QVERIFY(folder && !folder->hasSetupError());
        QTRY_VERIFY(folder->isReady());
        *_drives = {drive(u"personal-root"_s)};
        refreshGraph();
        QTRY_VERIFY_WITH_TIMEOUT(folder->canSync(), 3000);
        const auto rootCheck = folder->syncEngine().syncOptions()._localRootValid;
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, {u"pending/"_s});
        folder->journalDb()->commit(u"before physical root replacement"_s);
        QVERIFY(TestUtils::writeRandomFile(QDir(root).filePath(u"pending.txt"_s), 128));
        QVERIFY(QDir().rename(root, offline));
        QVERIFY(QDir().mkpath(root));
        QVERIFY(!folder->canSync());
        QVERIFY(!std::async(std::launch::async, rootCheck).get());
        QSignalSpy finished(&folder->syncEngine(), &SyncEngine::finished);
        folder->startSync();
        QCOMPARE(finished.size(), 0);
        QCOMPARE(folder->syncState(), SyncResult::SetupError);
        QVERIFY(QDir(root).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
        QVERIFY(QFileInfo::exists(QDir(offline).filePath(AtumRootBinding::journalName())));
        QVERIFY(QFileInfo::exists(QDir(offline).filePath(u"pending.txt"_s)));
        bool ok = false;
        QCOMPARE(folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, &ok), QSet<QString>{u"pending/"_s});
        QVERIFY(ok);
        QVERIFY(QDir(root).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
#endif
    }
};
QTEST_MAIN(TestAtumRootFolder)
#include "testatumrootfolder.moc"
