// SPDX-License-Identifier: GPL-2.0-or-later
#include "common/syncjournaldb.h"
#include "gui/atumrootbinding.h"
#include "libsync/accessmanager.h"
#include "libsync/csync_exclude.h"
#include "libsync/theme.h"
#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QProcess>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

using namespace OCC;
using namespace Qt::Literals::StringLiterals;
namespace {
AtumRootIdentity identity()
{
    return {u"https://drive.example.test"_s, u"https://auth.example.test/"_s, u"01M00000000000000000000000A"_s, u"personal-a"_s};
}
bool writeFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
QByteArray ownerOutcome(QProcess &process)
{
    QElapsedTimer timer;
    timer.start();
    QByteArray output;
    while (timer.elapsed() < 15000) {
        process.waitForReadyRead(100);
        output += process.readAllStandardOutput();
        if (output.contains("ROOT_OWNER=OWNED")) {
            return "OWNED";
        }
        if (output.contains("ROOT_OWNER=BUSY")) {
            return "BUSY";
        }
        if (process.state() == QProcess::NotRunning) {
            break;
        }
    }
    return output.right(512);
}
}

class TestAtumRootBinding : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void pinnedTransportRejectsForeignHeaderAndBodyBeforeSocket()
    {
        QTcpServer trap;
        QVERIFY(trap.listen(QHostAddress::LocalHost, 0));
        const OAuthIdentityProfile profile{QUrl(u"https://drive.example.test"_s), u"https://auth.example.test/"_s, u"client"_s, u"openid"_s};
        AccessManager manager(nullptr, &profile);
        QNetworkRequest request(QUrl(u"http://127.0.0.1:%1/upload"_s.arg(trap.serverPort())));
        request.setRawHeader("Authorization", "Bearer synthetic-secret");
        request.setRawHeader("Cookie", "synthetic-cookie");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QBuffer body;
        body.setData("synthetic private file bytes");
        QVERIFY(body.open(QIODevice::ReadOnly));
        const auto reply = manager.post(request, &body);
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        QVERIFY(reply->error() != QNetworkReply::NoError);
        QVERIFY(reply->request().url().isEmpty());
        QVERIFY(!reply->request().hasRawHeader("Authorization"));
        QVERIFY(!reply->request().hasRawHeader("Cookie"));
        QCOMPARE(body.pos(), qint64(0));
        QVERIFY(!trap.hasPendingConnections());
        const QList<QSslError> errors{QSslError(QSslError::SelfSignedCertificate)};
        QCOMPARE(manager.filterSslErrors(errors), errors);
        reply->deleteLater();
    }

    void freshAndMatchingResume()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        {
            auto owner = AtumRootBinding::acquire(dir.path(), identity(), true);
            QVERIFY2(owner, qPrintable(owner ? QString() : owner.error()));
            QVERIFY((*owner)->matches(identity()));
            SyncJournalDb db(QDir(dir.path()).filePath(AtumRootBinding::journalName()));
            QVERIFY(db.open());
            db.setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, {u"dirty-file"_s});
            db.commit(u"test"_s);
            QVERIFY(writeFile(QDir(dir.path()).filePath(u"dirty-file"_s), "keep me"));
            // A second object in the same process must lose before it can open SQLite.
            QVERIFY(!AtumRootBinding::acquire(dir.path(), identity(), false));
            db.close();
        }
        auto resumed = AtumRootBinding::acquire(dir.path(), identity(), false);
        QVERIFY(resumed);
        SyncJournalDb db(QDir(dir.path()).filePath(AtumRootBinding::journalName()));
        QVERIFY(db.open());
        bool ok = false;
        QCOMPARE(db.getSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, &ok), QSet<QString>{u"dirty-file/"_s});
        QVERIFY(ok);
        QCOMPARE(readFile(QDir(dir.path()).filePath(u"dirty-file"_s)), QByteArray("keep me"));
    }

    void refusesForeignBindingWithoutMutation_data()
    {
        QTest::addColumn<QString>("field");
        for (const auto &field : {u"origin"_s, u"issuer"_s, u"subject"_s, u"space"_s, u"canonicalRoot"_s, u"journal"_s, u"version"_s}) {
            QTest::newRow(qPrintable(field)) << field;
        }
    }
    void refusesForeignBindingWithoutMutation()
    {
        QFETCH(QString, field);
        QTemporaryDir dir;
        const auto marker = QDir(dir.path()).filePath(u".atum-drive-binding.json"_s);
        {
            auto owner = AtumRootBinding::acquire(dir.path(), identity(), true);
            QVERIFY(owner);
        }
        auto binding = QJsonDocument::fromJson(readFile(marker)).object();
        binding[field] = u"foreign"_s;
        const auto bytes = QJsonDocument(binding).toJson();
        QVERIFY(writeFile(marker, bytes));
        QVERIFY(writeFile(QDir(dir.path()).filePath(AtumRootBinding::journalName()), "existing journal"));
        QVERIFY(!AtumRootBinding::checkCandidate(dir.path(), identity()));
        QVERIFY(!AtumRootBinding::acquire(dir.path(), identity(), true));
        QCOMPARE(readFile(marker), bytes);
        QCOMPARE(readFile(QDir(dir.path()).filePath(AtumRootBinding::journalName())), QByteArray("existing journal"));
    }

    void unknownInventoryAndMissingJournalArePreserved()
    {
        QTemporaryDir dir;
        const auto file = QDir(dir.path()).filePath(u"work.txt"_s);
        QVERIFY(writeFile(file, "existing work"));
        QVERIFY(!AtumRootBinding::acquire(dir.path(), identity(), true));
        QVERIFY(!AtumRootBinding::checkCandidate(dir.path(), identity()));
        QCOMPARE(readFile(file), QByteArray("existing work"));
        QVERIFY(QFile::remove(file));
        {
            auto owner = AtumRootBinding::acquire(dir.path(), identity(), true);
            QVERIFY(owner);
        }
        QVERIFY(writeFile(file, "recovery requires inventory"));
        QVERIFY(!AtumRootBinding::checkCandidate(dir.path(), identity()));
        QVERIFY(!AtumRootBinding::acquire(dir.path(), identity(), false));
        QCOMPARE(readFile(file), QByteArray("recovery requires inventory"));
    }

    void rejectsHomeAndManagedRootsAndDoesNotCreateCandidate()
    {
        QVERIFY(!AtumRootBinding::checkCandidate(QDir::homePath(), identity()));
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkpath(u".hermes/skills"_s));
        QVERIFY(!AtumRootBinding::checkCandidate(QDir(dir.path()).filePath(u".hermes/skills"_s), identity()));
        const auto candidate = QDir(dir.path()).filePath(u"Atum"_s);
        QVERIFY(AtumRootBinding::checkCandidate(candidate, identity()));
        QVERIFY(!QFileInfo::exists(candidate));
    }

    void aliasAndSymlinkJournal()
    {
#ifdef Q_OS_WIN
        QSKIP("Windows symlink creation requires an additional OS privilege.");
#else
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkdir(u"root"_s));
        const auto root = QDir(dir.path()).filePath(u"root"_s);
        const auto alias = QDir(dir.path()).filePath(u"alias"_s);
        QVERIFY(QFile::link(root, alias));
        {
            auto owner = AtumRootBinding::acquire(root, identity(), true);
            QVERIFY(owner);
            QVERIFY(!AtumRootBinding::acquire(alias, identity(), true));
        }
        {
            auto resumed = AtumRootBinding::acquire(alias, identity(), false);
            QVERIFY(resumed);
            QVERIFY((*resumed)->matches(identity()));
            const auto external = QDir(dir.path()).filePath(u"external.db"_s);
            QVERIFY(writeFile(external, "external journal"));
            QVERIFY(QFile::link(external, QDir(root).filePath(AtumRootBinding::journalName())));
            QVERIFY(!(*resumed)->matches(identity()));
        }
        const auto external = QDir(dir.path()).filePath(u"external.db"_s);
        QVERIFY(!AtumRootBinding::acquire(root, identity(), false));
        QCOMPARE(readFile(external), QByteArray("external journal"));
#endif
    }

    void replacedRootCannotBeMistakenForOriginalOrUnlockedByOldOwner()
    {
#ifdef Q_OS_WIN
        QSKIP("Windows retains a directory handle that refuses live-root rename; replacement is covered on Unix.");
#else
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkdir(u"root"_s));
        const auto path = QDir(dir.path()).filePath(u"root"_s);
        auto acquired = AtumRootBinding::acquire(path, identity(), true);
        QVERIFY(acquired);
        auto oldOwner = *std::move(acquired);
        QVERIFY(oldOwner->matches(identity()));
        QVERIFY(QDir(dir.path()).rename(u"root"_s, u"disconnected-root"_s));
        QVERIFY(QDir(dir.path()).mkdir(u"root"_s));
        QVERIFY(!oldOwner->matches(identity()));
        auto replacement = AtumRootBinding::acquire(path, identity(), true);
        QVERIFY(replacement);
        oldOwner.reset();
        QVERIFY((*replacement)->matches(identity()));
        QVERIFY(!AtumRootBinding::acquire(path, identity(), true));
        QVERIFY(QFileInfo::exists(QDir(dir.path()).filePath(u"disconnected-root/.atum-drive-binding.json"_s)));
#endif
    }

    void mandatoryExclusionsSurviveReloadAndHiddenToggle()
    {
        ExcludedFiles files;
        files.setAtumRootExclusions();
        QTemporaryDir dir;
        const QString base = dir.path() + u'/';
        const QStringList denied = {u".atum-drive-binding.json"_s, u".atum-drive-owner.lock"_s, u".atum-drive-journal.db-wal"_s, u".env"_s,
            u"keys/private.pem"_s, u"auth/access-token"_s, u"db/live.sqlite3"_s, u".hermes/skills/paid/source.py"_s, u".protected-skills/purchase/source.py"_s,
            u"node_modules/tool/index.js"_s};
        for (const auto &path : denied) {
            const QString absolute = base + path;
            QVERIFY(QDir().mkpath(QFileInfo(absolute).path()));
            QVERIFY(writeFile(absolute, "synthetic excluded bytes"));
        }
        const QString allowedFile = base + u"documents/report.txt"_s;
        QVERIFY(QDir().mkpath(QFileInfo(allowedFile).path()));
        QVERIFY(writeFile(allowedFile, "ordinary document"));
        for (int pass = 0; pass < 2; ++pass) {
            for (bool hidden : {false, true}) {
                for (const auto &path : denied) {
                    const QString absolute = base + path;
                    QVERIFY2(files.isExcluded(absolute, base, hidden), qPrintable(path));
                }
                const QString allowed = base + u"documents/report.txt"_s;
                QVERIFY(!files.isExcluded(allowed, base, hidden));
            }
            files.clearManualExcludes();
            QVERIFY(files.reloadExcludeFiles());
        }
    }

    void twoCrashRecoveryContendersHaveOneOwner()
    {
        QTemporaryDir dir;
        QProcess first;
        first.start(QCoreApplication::applicationFilePath(), {u"--atum-root-owner"_s, dir.path()});
        QVERIFY(first.waitForStarted());
        QCOMPARE(ownerOutcome(first), QByteArray("OWNED"));
        // Killing only the process created by this test leaves a stale lock and a real SQLite journal.
        first.kill();
        QVERIFY(first.waitForFinished());
        QProcess a, b;
        a.start(QCoreApplication::applicationFilePath(), {u"--atum-root-owner"_s, dir.path()});
        b.start(QCoreApplication::applicationFilePath(), {u"--atum-root-owner"_s, dir.path()});
        QVERIFY(a.waitForStarted());
        QVERIFY(b.waitForStarted());
        const auto ar = ownerOutcome(a), br = ownerOutcome(b);
        QVERIFY2((ar == "OWNED" && br == "BUSY") || (br == "OWNED" && ar == "BUSY"), qPrintable(QString::fromUtf8(ar + '/' + br)));
        auto &winner = ar == "OWNED" ? a : b;
        auto &loser = ar == "OWNED" ? b : a;
        if (loser.state() != QProcess::NotRunning) {
            QVERIFY(loser.waitForFinished());
        }
        QCOMPARE(loser.exitCode(), 75);
        QVERIFY(!AtumRootBinding::acquire(dir.path(), identity(), false));
        winner.kill();
        QVERIFY(winner.waitForFinished());
        auto recovered = AtumRootBinding::acquire(dir.path(), identity(), false);
        QVERIFY(recovered);
        SyncJournalDb db(QDir(dir.path()).filePath(AtumRootBinding::journalName()));
        QVERIFY(db.open());
        bool ok = false;
        QCOMPARE(db.getSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, &ok), QSet<QString>{u"queued-after-crash/"_s});
        QVERIFY(ok);
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (argc == 3 && QString::fromLocal8Bit(argv[1]) == u"--atum-root-owner"_s) {
        auto owner = AtumRootBinding::acquire(QString::fromLocal8Bit(argv[2]), identity(), true);
        if (!owner) {
            QTextStream(stdout) << "ROOT_OWNER=BUSY" << Qt::endl;
            return 75;
        }
        SyncJournalDb db(QDir(QString::fromLocal8Bit(argv[2])).filePath(AtumRootBinding::journalName()));
        if (!db.open()) {
            return 80;
        }
        db.setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, {u"queued-after-crash"_s});
        db.commit(u"crash test"_s);
        QTextStream(stdout) << "ROOT_OWNER=OWNED" << Qt::endl;
        return app.exec();
    }
    TestAtumRootBinding tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "testatumrootbinding.moc"
