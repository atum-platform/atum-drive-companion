// SPDX-License-Identifier: GPL-2.0-or-later
#include "atumengine.h"
#include "accountmanager.h"
#include "atumengineprotocol.h"
#include "atumfilemetadata.h"
#include "atumrootbinding.h"
#include "fetchserversettings.h"
#include "folderman.h"
#include "libsync/configfile.h"
#include "libsync/csync_exclude.h"
#include "libsync/creds/oauth.h"
#include "libsync/graphapi/spacesmanager.h"
#include "libsync/progressdispatcher.h"
#include "libsync/syncengine.h"
#include "libsync/theme.h"
#include "networkinformation.h"
#include "newwizard/setupwizardaccountbuilder.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QSocketNotifier>
#include <QTimer>
#include <cstdio>
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace OCC {
namespace {
    void send(const QJsonObject &message)
    {
        const QByteArray line = AtumEngineLines::encode(message);
        if (line.isEmpty())
            return;
        fwrite(line.constData(), 1, size_t(line.size()), stdout);
        fflush(stdout);
    }
    void state(const QString &value)
    {
        send({{QStringLiteral("kind"), QStringLiteral("state")}, {QStringLiteral("state"), value}});
    }
}

int runAtumEngine()
{
#ifndef Q_OS_UNIX
    state(QStringLiteral("unavailable"));
    return 1;
#else
    const auto profile = Theme::instance()->oauthIdentityProfile();
    if (!profile || profile->scopes != QStringLiteral("openid")) {
        state(QStringLiteral("unavailable"));
        return 1;
    }
    qApp->setQuitOnLastWindowClosed(false);
    QObject lifetime;
    AtumEngineLines input;
    QStringList offered{QStringLiteral("progress"), QStringLiteral("sync"), QStringLiteral("quota")};
    // Preflight the full bundled rules before offering the capability. The
    // bound folder's actual snapshot (including user additions) is exported
    // only after enrollment, and only if its full load/export succeeds.
    ExcludedFiles bundledExclusions;
    bundledExclusions.addExcludeFilePath(ConfigFile::defaultExcludeFile());
    bundledExclusions.setAtumRootExclusions();
    const auto bundledPatterns = bundledExclusions.exclusionPatterns();
    if (bundledPatterns && atumEngineExclusions(*bundledPatterns))
        offered.append(QStringLiteral("exclusions"));
    AtumEngineFeatures features(offered);
    QString root;
    QString subject;
    AccountStatePtr accountState;
    std::unique_ptr<FolderMan> folders;
    bool started = false;
    bool forgetting = false;
    bool failed = false;
    bool stopping = false;
    QJsonObject previousProgress;
    qint64 lastSyncAt = 0;
    qint64 sentSyncAt = 0;
    QPointer<Folder> factsFolder;
    qint64 filesDone = 0, filesTotal = 0, bytesDone = 0, bytesTotal = 0;
    QSet<QString> excluded;
    bool runActive = false;
    bool upload = false, download = false;
    AtumEngineProgress progress(
        [&](const QJsonObject &message) {
            if (!features.accepted(QStringLiteral("progress")) || forgetting || failed || stopping)
                return;
            if (!atumEngineFactValid(message, previousProgress)) {
                failed = true;
                if (folders)
                    folders->setSyncEnabled(false);
                state(QStringLiteral("error"));
                return;
            }
            previousProgress = message;
            send(message);
        },
        &lifetime);

    const auto stop = [&] {
        stopping = true;
        progress.cancel();
        if (folders) {
            folders->setSyncEnabled(false);
            folders->unloadAndDeleteAllFolders();
        }
        AccountManager::instance()->shutdown();
        qApp->quit();
    };
    const auto fail = [&] {
        failed = true;
        progress.cancel();
        if (folders) {
            folders->setSyncEnabled(false);
        }
        state(QStringLiteral("error"));
    };
    const auto fact = [&](const QJsonObject &message) {
        if (!features.accepted(message.value(QStringLiteral("kind")).toString()) || forgetting || failed || stopping)
            return;
        if (!atumEngineFactValid(message, {}, sentSyncAt)) {
            fail();
            return;
        }
        if (message.value(QStringLiteral("kind")) == QStringLiteral("sync"))
            sentSyncAt = message.value(QStringLiteral("lastSyncAt")).toInteger();
        send(message);
    };
    const auto quota = [&] {
        if (!features.accepted(QStringLiteral("quota")) || !accountState || !accountState->isConnected() || !factsFolder || failed || forgetting || stopping)
            return;
        const auto *space = factsFolder->space();
        if (!space || space->disabled() || space->drive().getDriveType() != QStringLiteral("personal"))
            return;
        const auto value = space->drive().getQuota();
        if (!value.isValid() || !value.is_total_Set() || !value.is_used_Set() || !value.is_state_Set())
            return;
        const auto total = value.getTotal();
        const auto used = value.getUsed();
        if (total <= 0 || total > 9007199254740991LL || used < 0 || used > 9007199254740991LL)
            return;
        const QJsonObject message{{QStringLiteral("kind"), QStringLiteral("quota")}, {QStringLiteral("total"), total}, {QStringLiteral("used"), used},
            {QStringLiteral("remaining"), std::max(total - used, qint64(0))}, {QStringLiteral("state"), value.getState()}};
        // Unknown, unlimited or invalid server quotas are unavailable, not zero.
        if (atumEngineFactValid(message))
            fact(message);
    };
    const auto progressSnapshot = [&](bool active, const ProgressInfo *info = nullptr) {
        QJsonValue eta(QJsonValue::Null);
        if (info) {
            // Discovery and corrected estimates can shrink upstream totals;
            // retain the high-water marks for this run's monotonic snapshots.
            filesDone = std::max(filesDone, info->completedFiles());
            filesTotal = std::max({filesTotal, info->totalFiles(), filesDone});
            bytesDone = std::max(bytesDone, info->completedSize());
            bytesTotal = std::max({bytesTotal, info->totalSize(), bytesDone});
            if (active && info->isUpdatingEstimates() && info->trustEta()) {
                const auto milliseconds = info->totalProgress().estimatedEta;
                const auto seconds = milliseconds / 1000 + (milliseconds % 1000 != 0);
                if (seconds <= 9007199254740991ULL)
                    eta = qint64(seconds);
            }
            for (const auto &item : info->_currentItems) {
                upload |= item._item._direction == SyncFileItem::Up;
                download |= item._item._direction == SyncFileItem::Down;
            }
        }
        return QJsonObject{{QStringLiteral("kind"), QStringLiteral("progress")}, {QStringLiteral("active"), active},
            {QStringLiteral("files"), QJsonObject{{QStringLiteral("done"), filesDone}, {QStringLiteral("total"), filesTotal}}},
            {QStringLiteral("bytes"), QJsonObject{{QStringLiteral("done"), bytesDone}, {QStringLiteral("total"), bytesTotal}}},
            {QStringLiteral("etaSeconds"), eta},
            {QStringLiteral("direction"),
                upload && !download       ? QStringLiteral("up")
                    : download && !upload ? QStringLiteral("down")
                                          : QStringLiteral("mixed")}};
    };
    const auto attachFacts = [&] {
        if (!folders || folders->folders().size() != 1 || factsFolder)
            return;
        auto *folder = folders->folders().first();
        if (!folder->isReady())
            return;
        factsFolder = folder;
        if (features.accepted(QStringLiteral("exclusions"))) {
            const auto patterns = folder->syncEngine().exclusionPatterns();
            if (patterns) {
                const auto message = atumEngineExclusions(*patterns);
                if (message)
                    fact(*message);
            }
        }
        QObject::connect(folder, &Folder::syncStateChange, &lifetime, [&, folder] {
            if (folder->syncResult().status() != SyncResult::SyncRunning || runActive || failed || forgetting || stopping)
                return;
            filesDone = filesTotal = bytesDone = bytesTotal = 0;
            excluded.clear();
            upload = download = false;
            runActive = true;
            progress.beginRun();
            if (features.accepted(QStringLiteral("progress")))
                progress.update(progressSnapshot(true));
        });
        // Folder's own direct finished handler was connected by its constructor
        // first. Read its finalized SyncResult, while retaining the real success
        // bit (SyncResult alone can say Success after an aborted empty run).
        QObject::connect(
            &folder->syncEngine(), &SyncEngine::finished, &lifetime,
            [&, folder](bool success) {
                if (factsFolder != folder || !runActive || failed || forgetting || stopping)
                    return;
                runActive = false;
                if (features.accepted(QStringLiteral("progress")))
                    progress.update(progressSnapshot(false));
                const auto result = folder->syncResult();
                if (success && result.status() == SyncResult::Success && result.syncTime().isValid())
                    lastSyncAt = std::max(lastSyncAt, result.syncTime().toMSecsSinceEpoch());
                const auto conflicts =
                    std::max(qint64(result.numNewConflictItems()) + result.numOldConflictItems(), qint64(folder->journalDb()->conflictRecordPaths().size()));
                fact({{QStringLiteral("kind"), QStringLiteral("sync")}, {QStringLiteral("lastSyncAt"), lastSyncAt}, {QStringLiteral("conflicts"), conflicts},
                    {QStringLiteral("errors"), std::max(qint64(result.numErrorItems()), qint64(result.errorStrings().size()))},
                    {QStringLiteral("excluded"), result.numExcludedItems() + excluded.size()}});
            },
            Qt::DirectConnection);
        quota();
    };
    QObject::connect(ProgressDispatcher::instance(), &ProgressDispatcher::progressInfo, &lifetime, [&](Folder *folder, const ProgressInfo &info) {
        if (factsFolder == folder && runActive && features.accepted(QStringLiteral("progress")))
            progress.update(progressSnapshot(true, &info));
    });
    QObject::connect(ProgressDispatcher::instance(), &ProgressDispatcher::excluded, &lifetime, [&](Folder *folder, const QString &path) {
        if (factsFolder == folder && runActive && features.accepted(QStringLiteral("sync")))
            excluded.insert(path);
    });
    QObject::connect(ProgressDispatcher::instance(), &ProgressDispatcher::itemCompleted, &lifetime, [&](Folder *folder, const SyncFileItemPtr &item) {
        if (factsFolder == folder && runActive) {
            upload |= item->_direction == SyncFileItem::Up;
            download |= item->_direction == SyncFileItem::Down;
        }
    });
    const auto report = [&] {
        if (!accountState || forgetting) {
            return;
        }
        if (failed) {
            state(QStringLiteral("error"));
            return;
        }
        if (accountState->isSignedOut()) {
            state(QStringLiteral("reauthenticate"));
            return;
        }
        if (!folders || folders->folders().size() != 1) {
            state(QStringLiteral("connecting"));
            return;
        }
        if (!accountState->isConnected()) {
            state(QStringLiteral("offline"));
            return;
        }
        const auto status = folders->folders().first()->syncResult().status();
        switch (status) {
        case SyncResult::Success:
            state(QStringLiteral("ready"));
            break;
        case SyncResult::SyncRunning:
        case SyncResult::Queued:
            state(QStringLiteral("syncing"));
            break;
        case SyncResult::Undefined:
            state(QStringLiteral("connecting"));
            break;
        case SyncResult::Offline:
            state(QStringLiteral("offline"));
            break;
        case SyncResult::Paused:
            state(QStringLiteral("paused"));
            break;
        default:
            state(QStringLiteral("error"));
            break;
        }
    };
    const auto wire = [&] {
        QObject::connect(accountState.get(), &AccountState::stateChanged, &lifetime, [&](AccountState::State value) {
            if (value == AccountState::SignedOut)
                AccountManager::instance()->save();
        });
        QObject::connect(accountState.get(), &AccountState::isConnectedChanged, &lifetime, report);
        QObject::connect(accountState.get(), &AccountState::isConnectedChanged, &lifetime, quota);
        QObject::connect(accountState.get(), &AccountState::isConnectedChanged, folders.get(), &FolderMan::slotIsConnectedChanged);
        QObject::connect(folders.get(), &FolderMan::folderSyncStateChange, &lifetime, report);
        QObject::connect(folders.get(), &FolderMan::folderListChanged, &lifetime, attachFacts);
        QObject::connect(accountState->account()->spacesManager(), &GraphApi::SpacesManager::spaceChanged, &lifetime, [&](GraphApi::Space *) { quota(); });
        QObject::connect(accountState.get(), &AccountState::credentialCleanupChanged, &lifetime, [&] {
            if (forgetting && !accountState->credentialCleanupPending()) {
                state(accountState->credentialCleanupFailed() ? QStringLiteral("cleanup_failed") : QStringLiteral("disconnected"));
                stop();
            }
        });
    };
    const auto enroll = [&] {
        auto *spaces = accountState->account()->spacesManager();
        QObject::connect(
            spaces, &GraphApi::SpacesManager::ready, &lifetime,
            [&] {
                auto personal = accountState->account()->spacesManager()->spaces();
                personal.erase(std::remove_if(personal.begin(), personal.end(),
                                   [](const auto *space) { return space->disabled() || space->drive().getDriveType() != QStringLiteral("personal"); }),
                    personal.end());
                if (personal.size() != 1) {
                    fail();
                    return;
                }
                auto *space = personal.first();
                auto definition = FolderDefinition(
                    accountState->account()->uuid(), QUrl(space->drive().getRoot().getWebDavUrl()), space->drive().getRoot().getId(), space->displayName());
                definition.setLocalPath(root);
                if (!folders->addFolderFromWizard(accountState, std::move(definition), false)) {
                    fail();
                    return;
                }
                folders->setSyncEnabled(true);
                folders->scheduleAllFolders();
                report();
            },
            Qt::SingleShotConnection);
        accountState->checkConnectivity();
        spaces->checkReady();
    };

    QSocketNotifier reader(STDIN_FILENO, QSocketNotifier::Read);
    QObject::connect(&reader, &QSocketNotifier::activated, &lifetime, [&] {
        char bytes[4096];
        const auto length = ::read(STDIN_FILENO, bytes, sizeof(bytes));
        if (length <= 0) {
            stop();
            return;
        }
        const bool valid = input.append(QByteArray(bytes, int(length)), [&](const QJsonObject &request) {
            if (stopping)
                return false;
            const auto kind = request.value(QStringLiteral("kind")).toString();
            if (kind == QStringLiteral("stop")) {
                stop();
                return false;
            }
            if (kind == QStringLiteral("forget") && !accountState) {
                state(QStringLiteral("disconnected"));
                stop();
                return false;
            }
            if (kind == QStringLiteral("forget") && accountState && !forgetting) {
                forgetting = true;
                progress.cancel();
                folders->setSyncEnabled(false);
                accountState->signOutByUi();
                AccountManager::instance()->save();
                return true;
            }
            if (kind == QStringLiteral("file_metadata")) {
                if (!started) {
                    fail();
                    stop();
                    return false;
                }
                const auto result =
                    resolveAtumFileMetadata(!forgetting && folders && folders->folders().size() == 1 ? folders->folders().first() : nullptr, request);
                if (result.status == AtumFileMetadataStatus::Malformed) {
                    fail();
                    stop();
                    return false;
                }
                send(result.message);
                return true;
            }
            const bool cleanup = kind == QStringLiteral("cleanup");
            if ((kind != QStringLiteral("start") && !cleanup) || started) {
                fail();
                stop();
                return false;
            }
            if (!cleanup && !features.acceptStart(request))
                return false;
            started = true;
            subject = request.value(QStringLiteral("subject")).toString();
            root = request.value(QStringLiteral("root")).toString();
            const auto configDir = request.value(QStringLiteral("configDir")).toString();
            const QFileInfo rootInfo(root);
            const QFileInfo configInfo(configDir);
            if (request.value(QStringLiteral("issuer")).toString() != profile->issuer || subject.isEmpty() || subject.size() > 200
                || (!cleanup && (!rootInfo.isAbsolute() || !rootInfo.isDir() || rootInfo.isSymLink())) || !configInfo.isAbsolute() || !configInfo.isDir()
                || configInfo.isSymLink()
                || (!cleanup && !AtumRootBinding::checkCandidate(root, {profile->driveOrigin.toString(), profile->issuer, subject, {}}))
                || !ConfigFile::setConfDir(configDir)) {
                fail();
                stop();
                return false;
            }
            NetworkInformation::instance();
            folders = FolderMan::createInstance();
            folders->setSyncEnabled(false);
            if (!AccountManager::instance()->restore()) {
                fail();
                stop();
                return false;
            }
            const auto accounts = AccountManager::instance()->accounts();
            if (accounts.isEmpty() && cleanup) {
                state(QStringLiteral("disconnected"));
                stop();
                return false;
            }
            if (!accounts.isEmpty()) {
                if (accounts.size() != 1 || accounts.first()->account()->atumIssuer() != profile->issuer
                    || accounts.first()->account()->atumSubject() != subject || accounts.first()->account()->url() != profile->driveOrigin
                    || (!cleanup
                        && (!accounts.first()->account()->hasDefaultSyncRoot()
                            || QFileInfo(accounts.first()->account()->defaultSyncRoot()).canonicalFilePath() != rootInfo.canonicalFilePath()))) {
                    fail();
                    stop();
                    return false;
                }
                accountState = accounts.first();
                wire();
                if (cleanup) {
                    forgetting = true;
                    accountState->signOutByUi();
                    AccountManager::instance()->save();
                    return true;
                }
                if (!folders->loadFolders().has_value() || folders->folders().size() > 1) {
                    fail();
                    return true;
                }
                accountState->checkConnectivity();
                if (!accountState->isSignedOut()) {
                    if (folders->folders().isEmpty()) {
                        enroll();
                        return true;
                    }
                    folders->setSyncEnabled(true);
                    folders->scheduleAllFolders();
                    report();
                    return true;
                }
            }
            auto *manager = new QNetworkAccessManager(&lifetime);
            auto *oauth = new OAuth(profile->driveOrigin, manager, {}, &lifetime);
            QObject::connect(oauth, &OAuth::authorisationLinkChanged, &lifetime,
                [oauth] { send({{QStringLiteral("kind"), QStringLiteral("authorize")}, {QStringLiteral("url"), oauth->authorisationLink().toString()}}); });
            QObject::connect(oauth, &OAuth::result, &lifetime, [&, oauth](OAuth::Result result, const QString &token, const QString &refresh) {
                if (result != OAuth::LoggedIn || oauth->idToken().sub() != subject
                    || oauth->idToken().toJson().value(QStringLiteral("iss")).toString() != profile->issuer) {
                    fail();
                    oauth->deleteLater();
                    return;
                }
                if (accountState) {
                    auto *credentials = qobject_cast<HttpCredentialsGui *>(accountState->account()->credentials());
                    if (!credentials) {
                        fail();
                        return;
                    }
                    OAuth::persist(accountState->account(), oauth->dynamicRegistrationData(), oauth->idToken());
                    QObject::connect(
                        credentials, &HttpCredentials::credentialsStored, &lifetime,
                        [&](bool success) {
                            if (!success) {
                                fail();
                                return;
                            }
                            accountState->signIn();
                            if (folders->folders().isEmpty()) {
                                enroll();
                                return;
                            }
                            folders->setSyncEnabled(true);
                            folders->scheduleAllFolders();
                            AccountManager::instance()->save();
                        },
                        Qt::SingleShotConnection);
                    credentials->acceptNativeOAuth(token, refresh);
                    oauth->deleteLater();
                    return;
                }
                Wizard::SetupWizardAccountBuilder builder;
                builder.setServerUrl(profile->driveOrigin);
                builder.setSyncTargetDir(root);
                builder.setAuthenticationStrategy(
                    std::make_unique<Wizard::OAuth2AuthenticationStrategy>(token, refresh, oauth->dynamicRegistrationData(), oauth->idToken()));
                const auto pending = builder.build();
                // Persist the owner/account location before writing Keychain custody so
                // root-independent cleanup can recover an interrupted first sign-in.
                accountState = AccountManager::instance()->addAccount(pending);
                accountState->setSettingUp(true);
                wire();
                AccountManager::instance()->save();
                auto *credentials = qobject_cast<HttpCredentials *>(pending->credentials());
                QObject::connect(
                    credentials, &HttpCredentials::credentialsStored, &lifetime,
                    [&, pending](bool success) {
                        if (forgetting)
                            return;
                        if (!success) {
                            fail();
                            return;
                        }
                        auto *settings = new FetchServerSettingsJob(accountState->account(), &lifetime);
                        QObject::connect(settings, &FetchServerSettingsJob::finishedSignal, &lifetime, [&] {
                            AccountManager::instance()->save();
                            Q_EMIT accountState->account()->credentialsFetched();
                            accountState->setSettingUp(false);
                            enroll();
                        });
                        settings->start();
                    },
                    Qt::SingleShotConnection);
                credentials->persist();
                oauth->deleteLater();
            });
            state(QStringLiteral("authorizing"));
            oauth->startAuthentication();
            return true;
        });
        if (!valid && !stopping) {
            fail();
            stop();
        }
    });
    QTimer initial;
    initial.setSingleShot(true);
    QObject::connect(&initial, &QTimer::timeout, &lifetime, [&] {
        if (!started)
            stop();
    });
    initial.start(10000);
    QTimer heartbeat;
    heartbeat.setInterval(2000);
    QObject::connect(&heartbeat, &QTimer::timeout, &lifetime, report);
    heartbeat.start();
    send({{QStringLiteral("kind"), QStringLiteral("hello")}, {QStringLiteral("protocol"), 1}, {QStringLiteral("origin"), profile->driveOrigin.toString()},
        {QStringLiteral("issuer"), profile->issuer}, {QStringLiteral("taskFiles"), true},
        {QStringLiteral("features"), QJsonArray::fromStringList(features.offered())}});
    const int result = qApp->exec();
    if (folders) {
        folders->setSyncEnabled(false);
        folders->unloadAndDeleteAllFolders();
    }
    AccountManager::instance()->shutdown();
    return result;
#endif
}
}
