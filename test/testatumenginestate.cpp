// SPDX-License-Identifier: GPL-2.0-or-later
#include "gui/atumengine.h"
#include "libsync/theme.h"

#ifdef ATUM_ENGINE_OEM_PROFILE
#include "atumprofile.h"
#endif

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QTemporaryDir>
#include <QUrl>
#include <QUuid>
#include <QtTest>

using namespace OCC;

class TestAtumEngineState : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void mapsEveryNativeFolderState()
    {
        QCOMPARE(atumEngineFolderState(SyncResult::Success), QStringLiteral("ready"));
        QCOMPARE(atumEngineFolderState(SyncResult::Problem), QStringLiteral("ready"));
        QCOMPARE(atumEngineFolderState(SyncResult::SyncRunning), QStringLiteral("syncing"));
        QCOMPARE(atumEngineFolderState(SyncResult::Queued), QStringLiteral("syncing"));
        QCOMPARE(atumEngineFolderState(SyncResult::Undefined), QStringLiteral("connecting"));
        QCOMPARE(atumEngineFolderState(SyncResult::Offline), QStringLiteral("offline"));
        QCOMPARE(atumEngineFolderState(SyncResult::Paused), QStringLiteral("paused"));
        QCOMPARE(atumEngineFolderState(SyncResult::Error), QStringLiteral("error"));
        QCOMPARE(atumEngineFolderState(SyncResult::SetupError), QStringLiteral("error"));
    }

    void completedWithIgnoredFilesAdvancesSuccessTime()
    {
        QVERIFY(atumEngineRunSucceeded(true, SyncResult::Success));
        QVERIFY(atumEngineRunSucceeded(true, SyncResult::Problem));
        QVERIFY(!atumEngineRunSucceeded(false, SyncResult::Success));
        QVERIFY(!atumEngineRunSucceeded(false, SyncResult::Problem));
        QVERIFY(!atumEngineRunSucceeded(true, SyncResult::Error));
    }

    void zeroCredentialJobsSettleImmediately()
    {
        AtumEngineCleanupCompletion completion;

        const auto result = completion.finishCleanupIfSettled(false, false);
        QVERIFY(result);
        QCOMPARE(*result, QStringLiteral("disconnected"));
    }

    void asynchronousDeletionWaitsForAcknowledgement()
    {
        AtumEngineCleanupCompletion completion;

        QVERIFY(!completion.finishCleanupIfSettled(true, false));
        const auto result = completion.finishCleanupIfSettled(false, false);
        QVERIFY(result);
        QCOMPARE(*result, QStringLiteral("disconnected"));
    }

    void cleanupFailuresAreTerminal()
    {
        AtumEngineCleanupCompletion persistenceFailure;
        AtumEngineCleanupCompletion deletionFailure;

        const auto persistenceResult = persistenceFailure.finishCleanupIfSettled(false, true);
        QVERIFY(!deletionFailure.finishCleanupIfSettled(true, false));
        const auto deletionResult = deletionFailure.finishCleanupIfSettled(true, true);
        QVERIFY(persistenceResult);
        QVERIFY(deletionResult);
        QCOMPARE(*persistenceResult, QStringLiteral("cleanup_failed"));
        QCOMPARE(*deletionResult, QStringLiteral("cleanup_failed"));
    }

    void cleanupCompletionIsIdempotent()
    {
        AtumEngineCleanupCompletion completion;

        const auto result = completion.finishCleanupIfSettled(false, false);
        QVERIFY(result);
        QCOMPARE(*result, QStringLiteral("disconnected"));
        QVERIFY(!completion.finishCleanupIfSettled(false, false));
        QVERIFY(!completion.finishCleanupIfSettled(true, true));
    }

    void zeroCredentialCleanupProcessExitsOnceAndPreservesFiles()
    {
#ifndef Q_OS_UNIX
        QSKIP("The stdio Atum engine is unavailable on this platform.");
#elif !defined(ATUM_ENGINE_OEM_PROFILE)
        QSKIP("Requires a compiled Atum OEM profile; the unbranded engine has no cleanup authority.");
#else
        constexpr auto issuer = ATUM_MAS_ISSUER;
        constexpr auto origin = ATUM_DRIVE_ORIGIN;
        constexpr auto subject = "synthetic-zero-job-owner";
        const QByteArray preservedContents("synthetic user file\n");
        QTemporaryDir sandbox;
        QVERIFY(sandbox.isValid());

        QDir sandboxDir(sandbox.path());
        QVERIFY(sandboxDir.mkpath(QStringLiteral("config")));
        QVERIFY(sandboxDir.mkpath(QStringLiteral("home")));
        QVERIFY(sandboxDir.mkpath(QStringLiteral("tmp")));
        QVERIFY(sandboxDir.mkpath(QStringLiteral("Drive")));
        const auto configDir = sandboxDir.filePath(QStringLiteral("config"));
        const auto rootDir = sandboxDir.filePath(QStringLiteral("Drive"));
        const auto preservedPath = QDir(rootDir).filePath(QStringLiteral("preserved.txt"));

        QFile preserved(preservedPath);
        QVERIFY(preserved.open(QIODevice::WriteOnly));
        QCOMPARE(preserved.write(preservedContents), preservedContents.size());
        preserved.close();

        const auto configPath = QDir(configDir).filePath(Theme::instance()->configFileName());
        {
            QSettings settings(configPath, QSettings::IniFormat);
            settings.beginWriteArray(QStringLiteral("Accounts"), 1);
            settings.setArrayIndex(0);
            settings.setValue(QStringLiteral("url"), QUrl(QString::fromLatin1(origin)));
            settings.setValue(QStringLiteral("display-name"), QStringLiteral("Synthetic cleanup owner"));
            settings.setValue(QStringLiteral("uuid"), QUuid(QStringLiteral("{4f1c23f0-44bd-4c41-a32b-9dd4943e9b77}")));
            settings.setValue(QStringLiteral("atum-issuer"), QString::fromLatin1(issuer));
            settings.setValue(QStringLiteral("atum-subject"), QString::fromLatin1(subject));
            settings.setValue(QStringLiteral("default_sync_root"), rootDir);
            settings.endArray();
            settings.sync();
            QCOMPARE(settings.status(), QSettings::NoError);
            for (const auto &key : settings.allKeys()) {
                QVERIFY2(!key.startsWith(QStringLiteral("Credentials/")), qPrintable(key));
                QVERIFY2(!key.startsWith(QStringLiteral("CredentialOperations/")), qPrintable(key));
            }
        }

        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("HOME"), sandboxDir.filePath(QStringLiteral("home")));
        environment.insert(QStringLiteral("TMPDIR"), sandboxDir.filePath(QStringLiteral("tmp")));
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        process.setProcessEnvironment(environment);
        process.setProcessChannelMode(QProcess::SeparateChannels);
        process.start(QStringLiteral(ATUM_ENGINE_EXE), {QStringLiteral("--atum-engine")});
        QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));

        const QJsonObject request{{QStringLiteral("kind"), QStringLiteral("cleanup")}, {QStringLiteral("issuer"), QString::fromLatin1(issuer)},
            {QStringLiteral("subject"), QString::fromLatin1(subject)}, {QStringLiteral("configDir"), configDir}};
        const auto frame = QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n';
        QCOMPARE(process.write(frame), qint64(frame.size()));
        QVERIFY(process.waitForBytesWritten());
        // Exercise the race between the queued cleanup stop and stdin EOF.
        process.closeWriteChannel();
        if (!process.waitForFinished(10'000)) {
            const auto error = process.errorString();
            process.kill();
            process.waitForFinished();
            QFAIL(qPrintable(error));
        }

        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
        const auto standardOutput = process.readAllStandardOutput();
        const auto standardError = process.readAllStandardError();
        QList<QJsonObject> messages;
        for (const auto &line : standardOutput.split('\n')) {
            if (line.isEmpty())
                continue;
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(line, &error);
            QVERIFY2(error.error == QJsonParseError::NoError && document.isObject(), standardOutput.constData());
            messages.append(document.object());
        }
        QCOMPARE(messages.size(), 2);
        QCOMPARE(messages.at(0).value(QStringLiteral("kind")).toString(), QStringLiteral("hello"));
        QCOMPARE(messages.at(0).value(QStringLiteral("issuer")).toString(), QString::fromLatin1(issuer));
        QCOMPARE(messages.at(0).value(QStringLiteral("origin")).toString(), QString::fromLatin1(origin));
        QCOMPARE(messages.at(1), QJsonObject({{QStringLiteral("kind"), QStringLiteral("state")}, {QStringLiteral("state"), QStringLiteral("disconnected")}}));

        QFile preservedAfter(preservedPath);
        QVERIFY(preservedAfter.open(QIODevice::ReadOnly));
        QCOMPARE(preservedAfter.readAll(), preservedContents);
        QSettings settingsAfter(configPath, QSettings::IniFormat);
        QCOMPARE(settingsAfter.status(), QSettings::NoError);
        for (const auto &key : settingsAfter.allKeys()) {
            QVERIFY2(!key.startsWith(QStringLiteral("Credentials/")), qPrintable(key));
            QVERIFY2(!key.startsWith(QStringLiteral("CredentialOperations/")), qPrintable(key));
        }
        QVERIFY2(!standardError.toLower().contains("keychain"), standardError.constData());
#endif
    }
};

QTEST_GUILESS_MAIN(TestAtumEngineState)
#include "testatumenginestate.moc"
