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
};

QTEST_GUILESS_MAIN(TestAtumEngineState)
#include "testatumenginestate.moc"
