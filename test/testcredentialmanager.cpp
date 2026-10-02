/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */
#include "account.h"
#include "accountstate.h"
#include "libsync/creds/credentialmanager.h"
#include "libsync/creds/httpcredentials.h"
#include "theme.h"

#include "testutils/syncenginetestutils.h"

#include <QTest>
#include <QTimer>

namespace OCC {

class CustodyHttpCredentials : public HttpCredentials
{
public:
    CustodyHttpCredentials(const QString &accessToken, const QString &refreshToken)
    {
        _accessToken = accessToken;
        _refreshToken = refreshToken;
    }
    void restartOauth() override { }
};

class TestCredentialManager : public QObject
{
    Q_OBJECT

    bool _finished = false;

    QTemporaryFile _credStoreFile;
    QSettings _credStore;

    void setFallbackEnabled(QKeychain::Job *job)
    {
        // store the test credentials in a plain text settings file on unsupported platforms
        job->setSettings(&_credStore);
        job->setInsecureFallback(true);
    }

private Q_SLOTS:
    void init()
    {
        _finished = false;
        QVERIFY(_credStoreFile.open());
        _credStore.setPath(QSettings::IniFormat, QSettings::UserScope, _credStoreFile.fileName());
        _credStore.clear();
    }

    void testSetGet_data()
    {
        QTest::addColumn<QVariant>("data");

        QTest::newRow("bool") << QVariant::fromValue(true);
        QTest::newRow("int") << QVariant::fromValue(1);
        QTest::newRow("map") << QVariant::fromValue(
            QVariantMap{{QStringLiteral("foo"), QColor(Qt::red).name()}, {QStringLiteral("bar"), QStringLiteral("42")}});
    }

    void testSetGet()
    {
        QFETCH(QVariant, data);
        FakeFolder fakeFolder { FileInfo::A12_B12_C12_S12() };
        auto creds = fakeFolder.account()->credentialManager();

        const QString key = QStringLiteral("test");
        auto setJob = creds->set(key, data);
        setFallbackEnabled(setJob);

        connect(setJob, &QKeychain::Job::finished, this, [creds, data, key, setJob, this] {
#ifdef Q_OS_LINUX
            if (!qEnvironmentVariableIsSet("DBUS_SESSION_BUS_ADDRESS")) {
                QEXPECT_FAIL("", "QKeychain might not use the plaintext fallback and fail if dbus is not present", Abort);
                QCOMPARE(setJob->error(), QKeychain::NoError);
            }
#endif
            QCOMPARE(setJob->error(), QKeychain::NoError);
            auto getJob = creds->get(key);
            setFallbackEnabled(getJob->_job);
            connect(getJob, &CredentialJob::finished, this, [getJob, data, creds, this] {
                QCOMPARE(getJob->error(), QKeychain::NoError);
                QCOMPARE(getJob->data(), data);
                const auto jobs = creds->clear();
                for (auto &job : jobs) {
                    setFallbackEnabled(job);
                }
                connect(jobs[0], &QKeychain::Job::finished, this, [creds, data, jobs, this] {
                    QCOMPARE(jobs[0]->error(), QKeychain::NoError);
                    QVERIFY(creds->knownKeys().isEmpty());
                    _finished = true;
                });
            });
        });
#ifdef Q_OS_LINUX
        // As we have the skip condition on linux the wait might time out here.
        bool ok = QTest::qWaitFor([this] { return _finished; }, 30000);
        Q_UNUSED(ok)
#else
        QVERIFY(QTest::qWaitFor([this] { return _finished; }, 30000));
#endif
    }

    void testSetGet2()
    {
        FakeFolder fakeFolder { FileInfo::A12_B12_C12_S12() };
        auto creds = fakeFolder.account()->credentialManager();

        const QVariantMap data {
            { QStringLiteral("foo/test"), QColor(Qt::red) },
            { QStringLiteral("foo/test2"), QColor(Qt::gray) },
            { QStringLiteral("bar/test"), QColor(Qt::blue) },
            { QStringLiteral("narf/test"), QColor(Qt::green) }
        };

        QVector<QSignalSpy *> spies;
        for (auto it = data.cbegin(); it != data.cend(); ++it) {
            auto setJob = creds->set(it.key(), it.value());
            setFallbackEnabled(setJob);
            spies.append(new QSignalSpy(setJob, &QKeychain::Job::finished));
        }
        for (const auto s : spies) {
            QTRY_COMPARE_WITH_TIMEOUT(s->count(), 1, 30000);
        }
        qDeleteAll(spies);
        spies.clear();
        {
            auto jobs = creds->clear(QStringLiteral("foo"));
#ifdef Q_OS_LINUX
            if (!qEnvironmentVariableIsSet("DBUS_SESSION_BUS_ADDRESS")) {
                QEXPECT_FAIL("", "QKeychain might not use the plaintext fallback and fail if dbus is not present", Abort);
                QCOMPARE(jobs.size(), 2);
            }
#endif
            QCOMPARE(jobs.size(), 2);
            for (auto &job : jobs) {
                setFallbackEnabled(job);
                spies.append(new QSignalSpy(job, &QKeychain::Job::finished));
            }
            for (const auto s : spies) {
                QTRY_COMPARE_WITH_TIMEOUT(s->count(), 1, 30000);
            }
            qDeleteAll(spies);
        }
        // The group test must also remove its other synthetic OS entries.
        const auto remaining = creds->clear();
        for (const auto &job : remaining) {
            setFallbackEnabled(job);
        }
        QTRY_VERIFY_WITH_TIMEOUT(creds->knownKeys().isEmpty(), 30000);
    }

    void testReadyWaitsForSecureWrite()
    {
        FakeFolder folder{FileInfo::A12_B12_C12_S12()};
        auto account = folder.account();
        auto manager = account->credentialManager();
        QVector<QKeychain::Job *> jobs;
        manager->_startJob = [&jobs](QKeychain::Job *job) { jobs.append(job); };
        auto credentials = new CustodyHttpCredentials(QStringLiteral("synthetic-access"), QStringLiteral("synthetic-refresh"));
        account->setCredentials(credentials);
        QSignalSpy fetched(credentials, &AbstractCredentials::fetched);
        credentials->persist();
        QCOMPARE(jobs.size(), 1);
        QVERIFY(!credentials->ready());
        QVERIFY(!manager->contains(QStringLiteral("http/oauthtoken")));
        QVERIFY(credentials->refreshAccessToken()); // coalesces during storage
        QCOMPARE(jobs.size(), 1);
        auto reply = account->accessManager()->get(QNetworkRequest(QUrl(QStringLiteral("http://127.0.0.1:9/"))));
        QVERIFY(!reply->request().hasRawHeader("Authorization"));
        reply->abort();
        reply->deleteLater();
        QCOMPARE(fetched.count(), 0);
        jobs[0]->emitFinished();
        QVERIFY(credentials->ready());
        QVERIFY(manager->contains(QStringLiteral("http/oauthtoken")));
        QCOMPARE(fetched.count(), 1);
        reply = account->accessManager()->get(QNetworkRequest(QUrl(QStringLiteral("http://127.0.0.1:9/"))));
        QCOMPARE(reply->request().rawHeader("Authorization"), QByteArray("Bearer synthetic-access"));
        reply->abort();
        reply->deleteLater();
        // No actual OS write was made by this controlled acknowledgement seam.
        manager->credentialsList().clear();
    }

    void testWriteFailureStaysUnreadyAndRetainsFiles()
    {
        FakeFolder folder{FileInfo::A12_B12_C12_S12()};
        auto manager = folder.account()->credentialManager();
        QVector<QKeychain::Job *> jobs;
        manager->_startJob = [&jobs](QKeychain::Job *job) { jobs.append(job); };
        auto credentials = new CustodyHttpCredentials(QStringLiteral("synthetic-access"), QStringLiteral("synthetic-refresh"));
        folder.account()->setCredentials(credentials);
        QTemporaryFile localEdit;
        QVERIFY(localEdit.open());
        QCOMPARE(localEdit.write("local edit"), 10);
        QVERIFY(localEdit.flush());
        const auto before = folder.currentLocalState();
        QSignalSpy stored(credentials, &HttpCredentials::credentialsStored);
        credentials->persist();
        jobs[0]->emitFinishedWithError(QKeychain::AccessDeniedByUser, QStringLiteral("controlled keychain denial"));
        QVERIFY(!credentials->ready());
        QCOMPARE(stored.count(), 1);
        QCOMPARE(stored[0][0].toBool(), false);
        QVERIFY(manager->hasPendingOperation(QStringLiteral("http/oauthtoken")));
        QVERIFY(!manager->contains(QStringLiteral("http/oauthtoken")));
        QCOMPARE(folder.currentLocalState(), before);
        QVERIFY(localEdit.seek(0));
        QCOMPARE(localEdit.readAll(), QByteArray("local edit"));
        for (const auto &key : manager->operations().allKeys()) {
            QVERIFY(!manager->operations().value(key).toString().contains(QStringLiteral("synthetic-refresh")));
        }
        for (const auto &key : manager->credentialsList().allKeys()) {
            QVERIFY(!manager->credentialsList().value(key).toString().contains(QStringLiteral("synthetic-refresh")));
        }
        manager->operations().clear();
    }

    void testRotationFenceSurvivesRestart()
    {
        FakeFolder folder{FileInfo::A12_B12_C12_S12()};
        auto manager = folder.account()->credentialManager();
        const auto key = QStringLiteral("http/oauthtoken");
        manager->credentialsList().setValue(key, true);
        manager->credentialsList().sync();
        QVERIFY(manager->beginRotation(key));
        QVERIFY(!manager->beginRotation(key));
        CredentialManager restarted(folder.account().data());
        QVERIFY(restarted.hasPendingOperation(key));
        QVERIFY(!restarted.contains(key));
        auto read = restarted.get(key);
        QSignalSpy spy(read, &CredentialJob::finished);
        QVERIFY(!read->_job); // old rotating token was never handed to OS retrieval
        QTRY_COMPARE(spy.count(), 1);
        manager->credentialsList().clear();
        manager->operations().clear();
    }

    void testDeletionFailureRetainsFenceAndRetriesSameBinding()
    {
        FakeFolder folder{FileInfo::A12_B12_C12_S12()};
        auto manager = folder.account()->credentialManager();
        const auto key = QStringLiteral("http/oauthtoken");
        manager->credentialsList().setValue(key, true);
        QVector<QKeychain::Job *> jobs;
        manager->_startJob = [&jobs](QKeychain::Job *job) { jobs.append(job); };
        auto first = manager->remove(key);
        QVERIFY(!manager->contains(key));
        QVERIFY(manager->credentialsList().contains(key));
        QCOMPARE(manager->remove(key), first);
        QCOMPARE(jobs.size(), 1);
        const auto binding = first->key();
        first->emitFinishedWithError(QKeychain::CouldNotDeleteEntry, QStringLiteral("controlled delete denial"));
        QVERIFY(manager->hasPendingDeletion());
        QVERIFY(manager->deletionFailed());
        QVERIFY(manager->credentialsList().contains(key));
        folder.account()->setUrl(QUrl(QStringLiteral("https://changed.example.invalid")));
        auto retry = manager->remove(key);
        QCOMPARE(jobs.size(), 2);
        QCOMPARE(retry->key(), binding);
        retry->emitFinishedWithError(QKeychain::EntryNotFound, QStringLiteral("already deleted"));
        QVERIFY(!manager->hasPendingDeletion());
        QVERIFY(!manager->deletionFailed());
        QVERIFY(!manager->credentialsList().contains(key));
        QVERIFY(manager->knownKeys().isEmpty());
    }

    void testLateWriteAfterLogoutCannotResurrectCredentials()
    {
        FakeFolder folder{FileInfo::A12_B12_C12_S12()};
        auto manager = folder.account()->credentialManager();
        QVector<QKeychain::Job *> jobs;
        manager->_startJob = [&jobs](QKeychain::Job *job) { jobs.append(job); };
        auto credentials = new CustodyHttpCredentials(QStringLiteral("synthetic-access"), QStringLiteral("synthetic-refresh"));
        folder.account()->setCredentials(credentials);
        credentials->persist();
        credentials->forgetSensitiveData();
        QCOMPARE(jobs.size(), 2);
        jobs[1]->emitFinished(); // delete acknowledgement arrives before write
        QVERIFY(manager->hasPendingDeletion());
        jobs[0]->emitFinished(); // late write must trigger another delete
        QCOMPARE(jobs.size(), 3);
        QVERIFY(!credentials->ready());
        QVERIFY(manager->hasPendingDeletion());
        QVERIFY(!manager->contains(QStringLiteral("http/oauthtoken")));
        jobs[2]->emitFinished();
        QVERIFY(!manager->hasPendingDeletion());
        QVERIFY(manager->knownKeys().isEmpty());
    }

    void testSignedOutNotificationFollowsDurableCleanupFence()
    {
        FakeFolder folder{FileInfo::A12_B12_C12_S12()};
        auto account = folder.account();
        auto manager = account->credentialManager();
        manager->credentialsList().setValue(QStringLiteral("http/oauthtoken"), true);
        QVector<QKeychain::Job *> jobs;
        manager->_startJob = [&jobs](QKeychain::Job *job) { jobs.append(job); };
        account->setCredentials(new CustodyHttpCredentials(QStringLiteral("synthetic-access"), QStringLiteral("synthetic-refresh")));
        auto state = AccountState::fromNewAccount(account);
        bool fencedAtNotification = false;
        connect(state.get(), &AccountState::stateChanged, this, [&fencedAtNotification, manager](AccountState::State value) {
            if (value == AccountState::SignedOut) {
                fencedAtNotification = manager->hasPendingDeletion() && !manager->contains(QStringLiteral("http/oauthtoken"));
            }
        });
        state->signOutByUi();
        QVERIFY(fencedAtNotification);
        QVERIFY(state->credentialCleanupPending());
        state->signIn();
        QVERIFY(state->isSignedOut());
        QCOMPARE(jobs.size(), 1);
        jobs[0]->emitFinished();
        QVERIFY(!state->credentialCleanupPending());
        state->signIn();
        QVERIFY(!state->isSignedOut());
    }

    void testWriteTimeoutFencesLateSuccess()
    {
        FakeFolder folder{FileInfo::A12_B12_C12_S12()};
        auto manager = folder.account()->credentialManager();
        QVector<QKeychain::Job *> jobs;
        manager->_startJob = [&jobs](QKeychain::Job *job) { jobs.append(job); };
        auto credentials = new CustodyHttpCredentials(QStringLiteral("synthetic-access"), QStringLiteral("synthetic-refresh"));
        folder.account()->setCredentials(credentials);
        QSignalSpy stored(credentials, &HttpCredentials::credentialsStored);
        credentials->persist();
        auto timer = jobs[0]->findChild<QTimer *>();
        QVERIFY(timer);
        timer->stop(); // A single-shot timer is inactive when its timeout fires.
        QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
        QCOMPARE(stored.count(), 1);
        QCOMPARE(stored[0][0].toBool(), false);
        QVERIFY(!credentials->ready());
        QCOMPARE(jobs.size(), 2);
        QVERIFY(manager->hasPendingDeletion());
        // Exercise the other ordering: the late write precedes the delete ack.
        jobs[0]->emitFinished();
        jobs[1]->emitFinished();
        QCOMPARE(jobs.size(), 3);
        QVERIFY(manager->hasPendingDeletion());
        jobs[2]->emitFinished();
        QVERIFY(manager->knownKeys().isEmpty());
        QVERIFY(!credentials->ready());
        QCOMPARE(stored.count(), 1); // late success cannot publish readiness
    }

    void testDeleteTimeoutStaysFencedUntilAcknowledgement()
    {
        FakeFolder folder{FileInfo::A12_B12_C12_S12()};
        auto manager = folder.account()->credentialManager();
        const auto key = QStringLiteral("http/oauthtoken");
        manager->credentialsList().setValue(key, true);
        QVector<QKeychain::Job *> jobs;
        manager->_startJob = [&jobs](QKeychain::Job *job) { jobs.append(job); };
        auto deletion = manager->remove(key);
        auto timer = deletion->findChild<QTimer *>();
        QVERIFY(timer);
        timer->stop();
        QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
        QVERIFY(manager->deletionFailed());
        QVERIFY(manager->hasPendingDeletion());
        QVERIFY(manager->credentialsList().contains(key));
        QVERIFY(!manager->contains(key));
        QCOMPARE(manager->remove(key), deletion); // no competing OS delete
        QCOMPARE(jobs.size(), 1);
        deletion->emitFinished();
        QVERIFY(!manager->hasPendingDeletion());
        QVERIFY(!manager->deletionFailed());
        QVERIFY(manager->knownKeys().isEmpty());
    }

    void testCorruptSecureStoreReadCompletesWithError()
    {
        if (!QKeychain::isAvailable()) {
            QSKIP("Real OS keychain unavailable.");
        }
        FakeFolder folder{FileInfo::A12_B12_C12_S12()};
        auto manager = folder.account()->credentialManager();
        const auto key = QStringLiteral("custody/corrupt");
        auto write = manager->set(key, QStringLiteral("disposable"));
        QVERIFY(write);
        // QKeychain starts asynchronously; replace only this test-owned value.
        static_cast<QKeychain::WritePasswordJob *>(write)->setBinaryData(QByteArray::fromHex("ff"));
        QSignalSpy stored(manager, &CredentialManager::writeFinished);
        QVERIFY(stored.wait(30000));
        QVERIFY(stored[0][1].toBool());
        auto read = manager->get(key);
        QSignalSpy fetched(read, &CredentialJob::finished);
        bool failed = false;
        connect(read, &CredentialJob::finished, this, [read, &failed] { failed = read->error() == QKeychain::OtherError && !read->data().isValid(); });
        QVERIFY(fetched.wait(30000));
        QCOMPARE(fetched.count(), 1);
        QVERIFY(failed);
        auto deletion = manager->remove(key);
        QSignalSpy removed(deletion, &QKeychain::Job::finished);
        QVERIFY(removed.wait(30000));
        QVERIFY(manager->knownKeys().isEmpty());
    }

    void testSecureBackendAcknowledgesSyntheticWriteAndDelete()
    {
        if (!QKeychain::isAvailable()) {
            QSKIP("Real OS keychain unavailable; controlled custody seam tests still run.");
        }
        FakeFolder folder{FileInfo::A12_B12_C12_S12()};
        auto manager = folder.account()->credentialManager();
        const auto key = QStringLiteral("custody/synthetic");
        QSignalSpy stored(manager, &CredentialManager::writeFinished);
        auto write = manager->set(key, QStringLiteral("disposable keychain proof"));
        QVERIFY(write);
        QVERIFY(!write->insecureFallback());
        QVERIFY(!manager->contains(key));
        QVERIFY(stored.wait(30000));
        QCOMPARE(stored[0][1].toBool(), true);
        QVERIFY(manager->contains(key));
        auto deletion = manager->remove(key);
        QVERIFY(deletion);
        QVERIFY(!deletion->insecureFallback());
        QSignalSpy removed(deletion, &QKeychain::Job::finished);
        QVERIFY(removed.wait(30000));
        QVERIFY(!manager->hasPendingDeletion());
        QVERIFY(!manager->credentialsList().contains(key));
    }
};
}

// The Apple backend delivers completion on the main dispatch queue. It needs
// the native Cocoa event dispatcher, as used by the real Desktop application.
#if defined(Q_OS_MAC)
QTEST_MAIN(OCC::TestCredentialManager)
#else
QTEST_GUILESS_MAIN(OCC::TestCredentialManager)
#endif
#include "testcredentialmanager.moc"
