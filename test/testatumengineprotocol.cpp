// SPDX-License-Identifier: GPL-2.0-or-later
#include "gui/atumengineprotocol.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>
#include <QtTest>

using namespace OCC;

namespace {
QList<QJsonObject> vectors(const QString &name)
{
    QFile file(QStringLiteral(SOURCEDIR "/test/fixtures/atum-drive-engine-1.1/") + name + QStringLiteral(".jsonl"));
    if (!file.open(QIODevice::ReadOnly))
        qFatal("Cannot read canonical synthetic conformance pack");
    QList<QJsonObject> result;
    while (!file.atEnd()) {
        const auto document = QJsonDocument::fromJson(file.readLine());
        if (!document.isObject())
            qFatal("Invalid canonical fixture envelope");
        result.append(document.object());
    }
    return result;
}
QJsonObject progress(bool active, int done = 0, int total = 2)
{
    return {{QStringLiteral("kind"), QStringLiteral("progress")}, {QStringLiteral("active"), active},
        {QStringLiteral("files"), QJsonObject{{QStringLiteral("done"), done}, {QStringLiteral("total"), total}}},
        {QStringLiteral("bytes"), QJsonObject{{QStringLiteral("done"), done * 100}, {QStringLiteral("total"), total * 100}}},
        {QStringLiteral("etaSeconds"), QJsonValue(QJsonValue::Null)}, {QStringLiteral("direction"), QStringLiteral("mixed")}};
}
}

class TestAtumEngineProtocol : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void canonicalFraming_data()
    {
        QTest::addColumn<QJsonObject>("vector");
        for (const auto &name : {QStringLiteral("handshake"), QStringLiteral("facts"), QStringLiteral("trash"), QStringLiteral("invalid")}) {
            for (const auto &v : vectors(name))
                QTest::newRow(qPrintable(v.value(QStringLiteral("id")).toString())) << v;
        }
    }
    void canonicalFraming()
    {
        QFETCH(QJsonObject, vector);
        const auto wire = vector.value(QStringLiteral("wire")).toString().toUtf8();
        const auto code = vector.value(QStringLiteral("expected")).toObject().value(QStringLiteral("code")).toString();
        const bool structurallyValid =
            !QStringList{QStringLiteral("json"), QStringLiteral("object"), QStringLiteral("line_ending"), QStringLiteral("line_cap")}.contains(code);
        AtumEngineLines lines;
        bool valid = true;
        const auto consume = [](const QJsonObject &) { return true; };
        qsizetype offset = 0;
        const auto chunks = vector.value(QStringLiteral("chunks")).toArray();
        for (const auto size : chunks) {
            valid = lines.append(wire.mid(offset, size.toInt()), consume) && valid;
            offset += size.toInt();
        }
        if (chunks.isEmpty() || offset < wire.size())
            valid = lines.append(wire.mid(offset), consume) && valid;
        QCOMPARE(valid, structurallyValid);
    }
    void invalidUtf8AndStickyFailure()
    {
        AtumEngineLines lines;
        int consumed = 0;
        const auto consume = [&](const QJsonObject &) {
            ++consumed;
            return true;
        };
        QVERIFY(!lines.append(QByteArray("{\"kind\":\"") + char(0xff) + "\"}\n", consume));
        QVERIFY(!lines.append("{}\n", consume));
        QCOMPARE(consumed, 0);
    }
    void canonicalStartNegotiation_data()
    {
        QTest::addColumn<QJsonObject>("vector");
        for (const auto &v : vectors(QStringLiteral("handshake"))) {
            if (v.value(QStringLiteral("direction")) == QStringLiteral("app_to_engine"))
                QTest::newRow(qPrintable(v.value(QStringLiteral("id")).toString())) << v;
        }
    }
    void canonicalStartNegotiation()
    {
        QFETCH(QJsonObject, vector);
        QStringList offered;
        for (const auto value : vector.value(QStringLiteral("context")).toObject().value(QStringLiteral("offered")).toArray())
            offered.append(value.toString());
        AtumEngineFeatures features(offered);
        const auto request = QJsonDocument::fromJson(vector.value(QStringLiteral("wire")).toString().toUtf8()).object();
        const bool expected = vector.value(QStringLiteral("expected")).toObject().value(QStringLiteral("verdict")) == QStringLiteral("accept");
        QCOMPARE(features.acceptStart(request), expected);
        for (const auto &feature : offered) {
            QCOMPARE(features.accepted(feature), expected && request.value(QStringLiteral("features")).toArray().contains(feature));
        }
        if (expected)
            QVERIFY(!features.acceptStart(request));
    }
    void malformedAcceptanceDoesNotCommit()
    {
        for (const auto &value : {QJsonValue(true), QJsonValue(QStringLiteral("quota")),
                 QJsonValue(QJsonArray{QStringLiteral("quota"), QStringLiteral("quota")}), QJsonValue(QJsonArray{QStringLiteral("trash")})}) {
            AtumEngineFeatures features({QStringLiteral("quota")});
            QVERIFY(!features.accepted(QStringLiteral("quota")));
            QVERIFY(!features.acceptStart({{QStringLiteral("kind"), QStringLiteral("start")}, {QStringLiteral("features"), value}}));
            QVERIFY(features.acceptStart({{QStringLiteral("kind"), QStringLiteral("start")}}));
            QVERIFY(!features.accepted(QStringLiteral("quota")));
        }
    }
    void canonicalFactSchema_data()
    {
        QTest::addColumn<QJsonObject>("vector");
        for (const auto &name : {QStringLiteral("facts"), QStringLiteral("invalid")}) {
            for (const auto &v : vectors(name)) {
                // Receiver phase/direction/feature fences belong to S4-6;
                // this target verifies the actual sender's fact schema.
                const auto code = v.value(QStringLiteral("expected")).toObject().value(QStringLiteral("code")).toString();
                const auto document = QJsonDocument::fromJson(v.value(QStringLiteral("wire")).toString().toUtf8());
                const auto kind = document.object().value(QStringLiteral("kind")).toString();
                if (!QStringList{QStringLiteral("progress"), QStringLiteral("sync"), QStringLiteral("quota"), QStringLiteral("exclusions")}.contains(kind)
                    || QStringList{QStringLiteral("phase"), QStringLiteral("direction"), QStringLiteral("feature_not_accepted"), QStringLiteral("line_cap"),
                        QStringLiteral("line_ending")}
                        .contains(code))
                    continue;
                QTest::newRow(qPrintable(v.value(QStringLiteral("id")).toString())) << v;
            }
        }
    }
    void canonicalFactSchema()
    {
        QFETCH(QJsonObject, vector);
        const auto context = vector.value(QStringLiteral("context")).toObject();
        const auto wire = vector.value(QStringLiteral("wire")).toString().toUtf8();
        AtumEngineLines lines;
        const bool valid = lines.append(wire, [&](const QJsonObject &fact) {
            return atumEngineFactValid(
                fact, context.value(QStringLiteral("previousProgress")).toObject(), context.value(QStringLiteral("lastSyncAt")).toInteger());
        });
        QCOMPARE(valid, vector.value(QStringLiteral("expected")).toObject().value(QStringLiteral("verdict")) == QStringLiteral("accept"));
    }
    void emissionPacingAndTerminalSurvivesNextRun()
    {
        QList<QJsonObject> emitted;
        QList<qint64> times;
        QElapsedTimer clock;
        clock.start();
        AtumEngineProgress emitter(
            [&](const QJsonObject &snapshot) {
                emitted.append(snapshot);
                times.append(clock.elapsed());
            },
            this);
        emitter.beginRun();
        emitter.update(progress(true));
        QCOMPARE(emitted.size(), 1);
        emitter.update(progress(true, 1));
        emitter.update(progress(false, 1));
        emitter.beginRun();
        emitter.update(progress(true, 0, 3));
        QTRY_COMPARE_WITH_TIMEOUT(emitted.size(), 3, 2000);
        QCOMPARE(emitted[1], progress(false, 1));
        QCOMPARE(emitted[2], progress(true, 0, 3));
        for (int i = 1; i < times.size(); ++i)
            QVERIFY(times[i] - times[i - 1] >= 500);
        emitter.update(progress(false, 2, 3));
        emitter.cancel();
        QTest::qWait(550);
        QCOMPARE(emitted.size(), 3);
    }
    void blockedOutputCannotProduceImmediateBurst()
    {
        QList<qint64> writes;
        QElapsedTimer clock;
        clock.start();
        AtumEngineProgress emitter(
            [&](const QJsonObject &) {
                if (writes.isEmpty())
                    QThread::msleep(600); // Real stdout writes can wait for pipe capacity.
                writes.append(clock.elapsed());
            },
            this);
        emitter.beginRun();
        emitter.update(progress(true));
        emitter.update(progress(true, 1));
        QCOMPARE(writes.size(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(writes.size(), 2, 1000);
        QVERIFY(writes[1] - writes[0] >= 500);
    }
};

QTEST_GUILESS_MAIN(TestAtumEngineProtocol)
#include "testatumengineprotocol.moc"
