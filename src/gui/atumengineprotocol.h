// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <functional>
#include <optional>

namespace OCC {

// Shared by the real stdio reader and the focused QtCore conformance target.
// The bound is per line (including LF), never per OS read.
class AtumEngineLines
{
public:
    bool append(const QByteArray &bytes, const std::function<bool(const QJsonObject &)> &consume);
    static QByteArray encode(const QJsonObject &message);

private:
    QByteArray _pending;
    bool _failed = false;
};

class AtumEngineFeatures
{
public:
    explicit AtumEngineFeatures(const QStringList &offered);
    bool acceptStart(const QJsonObject &start);
    bool accepted(const QString &feature) const;
    QStringList offered() const;

private:
    QSet<QString> _offered;
    QSet<QString> _accepted;
    bool _started = false;
};

// Validate the presentation facts at the sender boundary. Native paths and
// legacy capability replies are deliberately outside this additive schema.
bool atumEngineFactValid(const QJsonObject &message, const QJsonObject &previousProgress = {}, qint64 lastSyncAt = 0);

// A complete presentation of the active native globs, or no authoritative
// export. Matching rules are never changed to make a frame fit the contract.
std::optional<QJsonObject> atumEngineExclusions(const QStringList &patterns);

// One pending terminal for the last visible run, plus the latest snapshot of
// the current run. Runs that finish before ever becoming visible can coalesce;
// a visible run's terminal can never be overwritten by the next run.
class AtumEngineProgress : public QObject
{
public:
    explicit AtumEngineProgress(std::function<void(const QJsonObject &)> callback, QObject *parent);
    void beginRun();
    void update(const QJsonObject &snapshot);
    void cancel();

private:
    void flush();
    void schedule();
    std::function<void(const QJsonObject &)> _emit;
    QTimer _timer;
    QElapsedTimer _sinceEmit;
    std::optional<QJsonObject> _terminal;
    std::optional<QJsonObject> _pending;
    bool _runVisible = false;
};
}
