// SPDX-License-Identifier: GPL-2.0-or-later
#include "atumengine.h"
#include "atumfilemetadata.h"
#include "accountmanager.h"
#include "atumrootbinding.h"
#include "fetchserversettings.h"
#include "folderman.h"
#include "libsync/configfile.h"
#include "libsync/creds/oauth.h"
#include "libsync/graphapi/spacesmanager.h"
#include "libsync/theme.h"
#include "networkinformation.h"
#include "newwizard/setupwizardaccountbuilder.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
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
        const QByteArray line = QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n';
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
    QByteArray input;
    QString root;
    QString subject;
    AccountStatePtr accountState;
    std::unique_ptr<FolderMan> folders;
    bool started = false;
    bool forgetting = false;
    bool failed = false;

    const auto stop = [&] {
        if (folders) {
            folders->setSyncEnabled(false);
            folders->unloadAndDeleteAllFolders();
        }
        AccountManager::instance()->shutdown();
        qApp->quit();
    };
    const auto fail = [&] {
        failed = true;
        if (folders) {
            folders->setSyncEnabled(false);
        }
        state(QStringLiteral("error"));
    };
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
        QObject::connect(accountState.get(), &AccountState::isConnectedChanged, folders.get(), &FolderMan::slotIsConnectedChanged);
        QObject::connect(folders.get(), &FolderMan::folderSyncStateChange, &lifetime, report);
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
        input.append(bytes, int(length));
        if (input.size() > 16384) {
            fail();
            stop();
            return;
        }
        while (input.contains('\n')) {
            const auto end = input.indexOf('\n');
            const auto line = input.left(end);
            input.remove(0, end + 1);
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(line, &error);
            const auto request = document.object();
            const auto kind = request.value(QStringLiteral("kind")).toString();
            if (error.error != QJsonParseError::NoError || !document.isObject()) {
                fail();
                stop();
                return;
            }
            if (kind == QStringLiteral("stop")) {
                stop();
                return;
            }
            if (kind == QStringLiteral("forget") && !accountState) {
                state(QStringLiteral("disconnected"));
                stop();
                return;
            }
            if (kind == QStringLiteral("forget") && accountState && !forgetting) {
                forgetting = true;
                folders->setSyncEnabled(false);
                accountState->signOutByUi();
                AccountManager::instance()->save();
                continue;
            }
            if (kind == QStringLiteral("file_metadata")) {
                if (!started) {
                    fail();
                    stop();
                    return;
                }
                const auto result = resolveAtumFileMetadata(!forgetting && folders && folders->folders().size() == 1 ? folders->folders().first() : nullptr, request);
                if (result.status == AtumFileMetadataStatus::Malformed) {
                    fail();
                    stop();
                    return;
                }
                send(result.message);
                continue;
            }
            const bool cleanup = kind == QStringLiteral("cleanup");
            if ((kind != QStringLiteral("start") && !cleanup) || started) {
                fail();
                stop();
                return;
            }
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
                return;
            }
            NetworkInformation::instance();
            folders = FolderMan::createInstance();
            folders->setSyncEnabled(false);
            if (!AccountManager::instance()->restore()) {
                fail();
                stop();
                return;
            }
            const auto accounts = AccountManager::instance()->accounts();
            if (accounts.isEmpty() && cleanup) {
                state(QStringLiteral("disconnected"));
                stop();
                return;
            }
            if (!accounts.isEmpty()) {
                if (accounts.size() != 1 || accounts.first()->account()->atumIssuer() != profile->issuer
                    || accounts.first()->account()->atumSubject() != subject || accounts.first()->account()->url() != profile->driveOrigin
                    || (!cleanup
                        && (!accounts.first()->account()->hasDefaultSyncRoot()
                            || QFileInfo(accounts.first()->account()->defaultSyncRoot()).canonicalFilePath() != rootInfo.canonicalFilePath()))) {
                    fail();
                    stop();
                    return;
                }
                accountState = accounts.first();
                wire();
                if (cleanup) {
                    forgetting = true;
                    accountState->signOutByUi();
                    AccountManager::instance()->save();
                    continue;
                }
                if (!folders->loadFolders().has_value() || folders->folders().size() > 1) {
                    fail();
                    return;
                }
                accountState->checkConnectivity();
                if (!accountState->isSignedOut()) {
                    if (folders->folders().isEmpty()) {
                        enroll();
                        continue;
                    }
                    folders->setSyncEnabled(true);
                    folders->scheduleAllFolders();
                    report();
                    continue;
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
        {QStringLiteral("issuer"), profile->issuer}, {QStringLiteral("taskFiles"), true}});
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
