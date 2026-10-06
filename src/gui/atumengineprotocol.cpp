// SPDX-License-Identifier: GPL-2.0-or-later
#include "atumengineprotocol.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QStringDecoder>
#include <algorithm>
#include <cmath>

namespace OCC {
namespace {
    constexpr qsizetype lineCap = 16384;
    constexpr double safeInteger = 9007199254740991.0;
    bool number(const QJsonValue &value)
    {
        return value.isDouble() && std::isfinite(value.toDouble()) && value.toDouble() >= 0 && value.toDouble() <= safeInteger
            && std::floor(value.toDouble()) == value.toDouble();
    }
    bool keys(const QJsonObject &object, const QStringList &expected)
    {
        auto actual = object.keys();
        auto wanted = expected;
        actual.sort();
        wanted.sort();
        return actual == wanted;
    }
    bool counter(const QJsonValue &value)
    {
        const auto object = value.toObject();
        return value.isObject() && keys(object, {QStringLiteral("done"), QStringLiteral("total")}) && number(object.value(QStringLiteral("done")))
            && number(object.value(QStringLiteral("total")))
            && object.value(QStringLiteral("done")).toDouble() <= object.value(QStringLiteral("total")).toDouble();
    }
    bool pattern(const QString &value)
    {
        if (value.isEmpty() || value.toUtf8().size() > 256 || value.startsWith('/') || value.startsWith('\\')
            || (value.size() >= 2 && value[0].isLetter() && value[1] == ':')) {
            return false;
        }
        for (auto c : value) {
            if (c.category() == QChar::Other_Control)
                return false;
        }
        auto normalized = value;
        normalized.replace('\\', '/');
        return !normalized.split('/').contains(QStringLiteral(".."));
    }
}

bool AtumEngineLines::append(const QByteArray &bytes, const std::function<bool(const QJsonObject &)> &consume)
{
    if (_failed)
        return false;
    // Consume as we append, so an arbitrarily large read containing small
    // complete frames never becomes an arbitrarily large pending allocation.
    qsizetype offset = 0;
    while (offset < bytes.size()) {
        const auto end = bytes.indexOf('\n', offset);
        const auto size = (end < 0 ? bytes.size() : end) - offset;
        if (_pending.size() + size + (end < 0 ? 0 : 1) > lineCap) {
            _failed = true;
            return false;
        }
        _pending.append(bytes.constData() + offset, size);
        offset += size;
        if (end < 0)
            return true;
        ++offset;
        QStringDecoder utf8(QStringDecoder::Utf8);
        const QString decoded = utf8.decode(_pending);
        Q_UNUSED(decoded);
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(_pending, &error);
        if (_pending.endsWith('\r') || utf8.hasError() || error.error != QJsonParseError::NoError || !document.isObject() || !consume(document.object())) {
            _failed = true;
            return false;
        }
        _pending.clear();
    }
    return true;
}

QByteArray AtumEngineLines::encode(const QJsonObject &message)
{
    const auto line = QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n';
    return line.size() <= lineCap ? line : QByteArray();
}

AtumEngineFeatures::AtumEngineFeatures(const QStringList &offered)
    : _offered(offered.begin(), offered.end())
{
}

bool AtumEngineFeatures::acceptStart(const QJsonObject &start)
{
    if (_started || start.value(QStringLiteral("kind")) != QStringLiteral("start"))
        return false;
    QSet<QString> accepted;
    const auto value = start.value(QStringLiteral("features"));
    if (!value.isUndefined()) {
        if (!value.isArray() || value.toArray().size() > 8)
            return false;
        for (const auto feature : value.toArray()) {
            if (!feature.isString() || !_offered.contains(feature.toString()) || accepted.contains(feature.toString()))
                return false;
            accepted.insert(feature.toString());
        }
    }
    _accepted = accepted;
    _started = true;
    return true;
}

bool AtumEngineFeatures::accepted(const QString &feature) const
{
    return _started && _accepted.contains(feature);
}

QStringList AtumEngineFeatures::offered() const
{
    QStringList result(_offered.begin(), _offered.end());
    result.sort();
    return result;
}

bool atumEngineFactValid(const QJsonObject &m, const QJsonObject &previousProgress, qint64 lastSyncAt)
{
    if (AtumEngineLines::encode(m).isEmpty())
        return false;
    const auto kind = m.value(QStringLiteral("kind")).toString();
    if (kind == QStringLiteral("progress")) {
        if (!keys(m,
                {QStringLiteral("kind"), QStringLiteral("active"), QStringLiteral("files"), QStringLiteral("bytes"), QStringLiteral("etaSeconds"),
                    QStringLiteral("direction")})
            || !m.value(QStringLiteral("active")).isBool() || !counter(m.value(QStringLiteral("files"))) || !counter(m.value(QStringLiteral("bytes")))
            || (!m.value(QStringLiteral("etaSeconds")).isNull() && !number(m.value(QStringLiteral("etaSeconds")))))
            return false;
        if (!QStringList{QStringLiteral("up"), QStringLiteral("down"), QStringLiteral("mixed")}.contains(m.value(QStringLiteral("direction")).toString()))
            return false;
        if (previousProgress.value(QStringLiteral("active")).toBool()) {
            for (const auto &field : {QStringLiteral("files"), QStringLiteral("bytes")}) {
                for (const auto &key : {QStringLiteral("done"), QStringLiteral("total")}) {
                    if (m.value(field).toObject().value(key).toDouble() < previousProgress.value(field).toObject().value(key).toDouble())
                        return false;
                }
            }
        }
        return true;
    }
    if (kind == QStringLiteral("sync")) {
        return keys(
                   m, {QStringLiteral("kind"), QStringLiteral("lastSyncAt"), QStringLiteral("conflicts"), QStringLiteral("errors"), QStringLiteral("excluded")})
            && number(m.value(QStringLiteral("lastSyncAt"))) && number(m.value(QStringLiteral("conflicts"))) && number(m.value(QStringLiteral("errors")))
            && number(m.value(QStringLiteral("excluded"))) && m.value(QStringLiteral("lastSyncAt")).toDouble() >= lastSyncAt;
    }
    if (kind == QStringLiteral("quota")) {
        if (!keys(m, {QStringLiteral("kind"), QStringLiteral("total"), QStringLiteral("used"), QStringLiteral("remaining"), QStringLiteral("state")})
            || !number(m.value(QStringLiteral("total"))) || !number(m.value(QStringLiteral("used"))) || !number(m.value(QStringLiteral("remaining"))))
            return false;
        const auto total = m.value(QStringLiteral("total")).toDouble();
        const auto used = m.value(QStringLiteral("used")).toDouble();
        const auto state = m.value(QStringLiteral("state")).toString();
        return total > 0 && m.value(QStringLiteral("remaining")).toDouble() == std::max(total - used, 0.0)
            && QStringList{QStringLiteral("normal"), QStringLiteral("nearing"), QStringLiteral("critical"), QStringLiteral("exceeded")}.contains(state)
            && (state == QStringLiteral("exceeded")) == (used >= total);
    }
    if (kind == QStringLiteral("exclusions")) {
        const auto patterns = m.value(QStringLiteral("patterns"));
        if (!keys(m, {QStringLiteral("kind"), QStringLiteral("patterns")}) || !patterns.isArray() || patterns.toArray().isEmpty()
            || patterns.toArray().size() > 256)
            return false;
        QSet<QString> seen;
        for (const auto p : patterns.toArray()) {
            if (!p.isString() || !pattern(p.toString()) || seen.contains(p.toString()))
                return false;
            seen.insert(p.toString());
        }
        return true;
    }
    return false;
}

AtumEngineProgress::AtumEngineProgress(std::function<void(const QJsonObject &)> callback, QObject *parent)
    : QObject(parent)
    , _emit(std::move(callback))
{
    _timer.setSingleShot(true);
    _timer.setTimerType(Qt::PreciseTimer);
    connect(&_timer, &QTimer::timeout, this, [this] { flush(); });
}

void AtumEngineProgress::beginRun()
{
    _pending.reset();
    _runVisible = false;
}

void AtumEngineProgress::update(const QJsonObject &snapshot)
{
    if (!snapshot.value(QStringLiteral("active")).toBool() && _runVisible) {
        _terminal = snapshot;
        _pending.reset();
        _runVisible = false;
    } else if (!snapshot.value(QStringLiteral("active")).toBool() && _terminal) {
        // The current short run was never visible. Retain the prior run's
        // terminal; its sync result is reported independently.
        _pending.reset();
    } else {
        _pending = snapshot;
    }
    schedule();
}

void AtumEngineProgress::schedule()
{
    if (!_terminal && !_pending)
        return;
    const auto elapsed = _sinceEmit.isValid() ? _sinceEmit.elapsed() : 500;
    if (elapsed >= 500)
        flush();
    else
        _timer.start(int(500 - elapsed));
}

void AtumEngineProgress::flush()
{
    if (_sinceEmit.isValid() && _sinceEmit.elapsed() < 500) {
        schedule();
        return;
    }
    std::optional<QJsonObject> next;
    if (_terminal) {
        next = _terminal;
        _terminal.reset();
    } else if (_pending) {
        next = _pending;
        _pending.reset();
        _runVisible = next->value(QStringLiteral("active")).toBool();
    }
    if (next) {
        _sinceEmit.start();
        _emit(*next);
    }
    schedule();
}

void AtumEngineProgress::cancel()
{
    _timer.stop();
    _pending.reset();
    _terminal.reset();
    _runVisible = false;
}
}
