// SPDX-License-Identifier: GPL-2.0-or-later
#include "gui/atumengine.h"

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
};

QTEST_GUILESS_MAIN(TestAtumEngineState)
#include "testatumenginestate.moc"
