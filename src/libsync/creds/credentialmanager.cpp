#include "credentialmanager.h"

#include "account.h"
#include "configfile.h"
#include "theme.h"

#include "common/asserts.h"
#include "common/chronoelapsedtimer.h"

#include <QCborValue>
#include <QLoggingCategory>
#include <QTimer>

#include <algorithm>
#include <chrono>

using namespace std::chrono_literals;

using namespace OCC;

Q_LOGGING_CATEGORY(lcCredentialsManager, "sync.credentials.manager", QtDebugMsg)

namespace {
constexpr auto timeoutC = 30s;
QString credentialKeyC()
{
    return QStringLiteral("%1_credentials").arg(Theme::instance()->appName());
}

QString accountKey(const Account *acc)
{
    OC_ASSERT(!acc->url().isEmpty());
    return QStringLiteral("%1:%2:%3").arg(credentialKeyC(), acc->url().host(), acc->uuid().toString(QUuid::WithoutBraces));
}

QString scope(const CredentialManager *const manager)
{
    return manager->account() ? accountKey(manager->account()) : credentialKeyC();
}

QString scopedKey(const CredentialManager *const manager, const QString &key)
{
    return scope(manager) + QLatin1Char(':') + key;
}
}

CredentialManager::CredentialManager(Account *acc)
    : QObject(acc)
    , _account(acc)
{
}

CredentialManager::CredentialManager(QObject *parent)
    : QObject(parent)
{
}


CredentialJob *CredentialManager::get(const QString &key)
{
    qCInfo(lcCredentialsManager) << u"get" << scopedKey(this, key);
    auto out = new CredentialJob(this, key);
    out->start();
    return out;
}

QKeychain::Job *CredentialManager::set(const QString &key, const QVariant &data)
{
    OC_ASSERT(!data.isNull());
    if (_writes.contains(key) || operations().value(QStringLiteral("pending/") + key) == QStringLiteral("delete")
        || !markPending(key, QStringLiteral("write"))) {
        return nullptr;
    }
    const auto generation = ++_generations[key];
    const auto boundKey = binding() + QLatin1Char(':') + key;
    auto writeJob = new QKeychain::WritePasswordJob(Theme::instance()->appName());
    writeJob->setKey(boundKey);
    _writes.insert(key, writeJob);
    if (_account) {
        // Custody cleanup must outlive a cancelled enrollment window.
        connect(writeJob, &QKeychain::Job::finished, writeJob, [account = _account->sharedFromThis()] { });
    }

    auto timer = new QTimer(writeJob);
    timer->setSingleShot(true);
    timer->setInterval(timeoutC);

    Utility::ChronoElapsedTimer elapsedTimer;
    connect(timer, &QTimer::timeout, this, [this, key, writeJob] {
        // The OS operation cannot be cancelled. Fence a late success and delete it.
        Q_EMIT writeFinished(writeJob, false);
        remove(key);
    });
    connect(writeJob, &QKeychain::WritePasswordJob::finished, this, [writeJob, key, generation, timer, elapsedTimer, this] {
        const bool timedOut = !timer->isActive();
        timer->stop();
        _writes.remove(key);
        const bool current = generation == _generations.value(key);
        if (!current) {
            // A logout may have completed its delete before this write completed.
            if (_deletes.contains(key)) {
                _deleteAgain.insert(key);
            } else {
                remove(key);
            }
        } else if (writeJob->error() == QKeychain::NoError) {
            qCInfo(lcCredentialsManager) << u"added" << writeJob->key() << u"after" << elapsedTimer;
            credentialsList().setValue(key, true);
            credentialsList().sync();
            if (credentialsList().status() == QSettings::NoError) {
                operations().remove(QStringLiteral("pending/") + key);
                operations().sync();
            }
        } else {
            qCWarning(lcCredentialsManager) << u"Failed to set:" << writeJob->key() << writeJob->errorString() << u"after" << elapsedTimer;
        }
        Q_EMIT stateChanged();
        if (!timedOut) {
            Q_EMIT writeFinished(writeJob, current && writeJob->error() == QKeychain::NoError && contains(key));
        }
    });
    writeJob->setBinaryData(QCborValue::fromVariant(data).toCbor());
    // start is delayed so we can directly call it
    _startJob(writeJob);
    timer->start();

    return writeJob;
}

QKeychain::Job *CredentialManager::remove(const QString &key)
{
    if (_deletes.contains(key)) {
        return _deletes.value(key);
    }
    ++_generations[key];
    if (!markPending(key, QStringLiteral("delete"))) {
        return nullptr;
    }
    operations().remove(QStringLiteral("failed/") + key);
    operations().sync();
    auto keychainJob = new QKeychain::DeletePasswordJob(Theme::instance()->appName());
    keychainJob->setKey(binding() + QLatin1Char(':') + key);
    _deletes.insert(key, keychainJob);
    if (_account) {
        connect(keychainJob, &QKeychain::Job::finished, keychainJob, [account = _account->sharedFromThis()] { });
    }
    auto timer = new QTimer(keychainJob);
    timer->setSingleShot(true);
    timer->setInterval(timeoutC);
    connect(timer, &QTimer::timeout, this, [this, key] {
        operations().setValue(QStringLiteral("failed/") + key, true);
        operations().sync();
        Q_EMIT stateChanged();
    });
    connect(keychainJob, &QKeychain::DeletePasswordJob::finished, this, [keychainJob, key, timer, this] {
        timer->stop();
        _deletes.remove(key);
        if (_deleteAgain.remove(key)) {
            remove(key);
            return;
        }
        if (keychainJob->error() == QKeychain::NoError || keychainJob->error() == QKeychain::EntryNotFound) {
            // An unacknowledged write can still recreate this key. Keep its fence.
            if (!_writes.contains(key)) {
                credentialsList().remove(key);
                credentialsList().sync();
                if (credentialsList().status() == QSettings::NoError) {
                    operations().remove(QStringLiteral("pending/") + key);
                    operations().remove(QStringLiteral("failed/") + key);
                    operations().sync();
                }
            }
        } else {
            operations().setValue(QStringLiteral("failed/") + key, true);
            operations().sync();
            qCWarning(lcCredentialsManager) << u"Failed to remove:" << keychainJob->key() << keychainJob->errorString();
        }
        Q_EMIT stateChanged();
    });
    // start is delayed so we can directly call it
    _startJob(keychainJob);
    timer->start();
    Q_EMIT stateChanged();
    return keychainJob;
}

QVector<QPointer<QKeychain::Job>> CredentialManager::clear(const QString &group)
{
    OC_ENFORCE(_account || !group.isEmpty());
    const auto keys = knownKeys(group);
    QVector<QPointer<QKeychain::Job>> out;
    out.reserve(keys.size());
    for (const auto &key : keys) {
        out << remove(key);
    }
    return out;
}

const Account *CredentialManager::account() const
{
    return _account;
}

bool CredentialManager::contains(const QString &key) const
{
    return credentialsList().contains(key) && !hasPendingOperation(key) && credentialsList().status() == QSettings::NoError
        && operations().status() == QSettings::NoError;
}

QStringList CredentialManager::knownKeys(const QString &group) const
{
    auto keys = credentialsList().allKeys();
    keys.append(pendingKeys());
    keys.removeDuplicates();
    QStringList out;
    for (const auto &k : keys) {
        if (group.isEmpty() || k.startsWith(group + QLatin1Char('/'))) {
            out.append(k);
        }
    }
    return out;
}

QString CredentialManager::binding() const
{
    if (_binding.isEmpty()) {
        _binding = scope(this);
    }
    return _binding;
}

QSettings &CredentialManager::operations() const
{
    if (!_operations) {
        _operations = ConfigFile::makeQSettingsPointer();
        _operations->beginGroup(QStringLiteral("CredentialOperations/") + binding());
    }
    return *_operations;
}

bool CredentialManager::markPending(const QString &key, const QString &operation)
{
    operations().setValue(QStringLiteral("pending/") + key, operation);
    operations().sync();
    Q_EMIT stateChanged();
    return operations().status() == QSettings::NoError;
}

bool CredentialManager::beginRotation(const QString &key)
{
    return !hasPendingOperation(key) && markPending(key, QStringLiteral("rotation"));
}

bool CredentialManager::hasPendingOperation(const QString &key) const
{
    return operations().contains(QStringLiteral("pending/") + key);
}

QStringList CredentialManager::pendingKeys() const
{
    operations().beginGroup(QStringLiteral("pending"));
    const auto keys = operations().allKeys();
    operations().endGroup();
    return keys;
}

bool CredentialManager::hasPendingDeletion() const
{
    const auto keys = pendingKeys();
    return std::any_of(
        keys.cbegin(), keys.cend(), [this](const QString &key) { return operations().value(QStringLiteral("pending/") + key) == QStringLiteral("delete"); });
}

bool CredentialManager::deletionFailed() const
{
    if (hasPendingDeletion() && operations().status() != QSettings::NoError) {
        return true;
    }
    const auto keys = pendingKeys();
    return std::any_of(keys.cbegin(), keys.cend(), [this](const QString &key) {
        return operations().value(QStringLiteral("pending/") + key) == QStringLiteral("delete") && operations().value(QStringLiteral("failed/") + key).toBool();
    });
}

/**
 * Utility function to lazily create the settings (group).
 *
 * IMPORTANT: the underlying storage is a std::unique_ptr, so do *NOT* store this reference anywhere!
 */
QSettings &CredentialManager::credentialsList() const
{
    // delayed init as scope requires a fully inizialised acc
    if (!_credentialsList) {
        _credentialsList = ConfigFile::makeQSettingsPointer();
        _credentialsList->beginGroup(QStringLiteral("Credentials/") + binding());
    }
    return *_credentialsList;
}

CredentialJob::CredentialJob(CredentialManager *parent, const QString &key)
    : QObject(parent)
    , _key(key)
    , _parent(parent)
{
    connect(this, &CredentialJob::finished, this, &CredentialJob::deleteLater);
}

QString CredentialJob::errorString() const
{
    return _errorString;
}

const QVariant &CredentialJob::data() const
{
    return _data;
}

QKeychain::Error CredentialJob::error() const
{
    return _error;
}

void CredentialJob::start()
{
    if (!_parent->contains(_key)) {
        _error = QKeychain::EntryNotFound;
        // QKeychain is started delayed, Q_EMIT the signal delayed to make sure we are connected
        qCDebug(lcCredentialsManager) << u"We don't know" << _key << u"skipping retrieval from keychain";
        QTimer::singleShot(0, this, &CredentialJob::finished);
        return;
    }

    _job = new QKeychain::ReadPasswordJob(Theme::instance()->appName());
    _job->setKey(_parent->binding() + QLatin1Char(':') + _key);
    connect(_job, &QKeychain::ReadPasswordJob::finished, this, [this] {
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
        if (_retryOnKeyChainError && (_job->error() == QKeychain::NoBackendAvailable || _job->error() == QKeychain::OtherError)) {
            // Could be that the backend was not yet available. Wait some extra seconds.
            // (Issues #4274 and #6522)
            // (For kwallet, the error is OtherError instead of NoBackendAvailable, maybe a bug in QtKeychain)
            qCInfo(lcCredentialsManager) << u"Backend unavailable (yet?) Retrying in a few seconds." << _job->errorString();
            QTimer::singleShot(10s, this, &CredentialJob::start);
            _retryOnKeyChainError = false;
            return;
        }
#endif
        if (_parent->hasPendingOperation(_key)) {
            _error = QKeychain::EntryNotFound;
            Q_EMIT finished();
            return;
        }
        if (_job->error() == QKeychain::NoError) {
            QCborParserError error;
            const auto obj = QCborValue::fromCbor(_job->binaryData(), &error);
            if (error.error != QCborError::NoError) {
                _error = QKeychain::OtherError;
                _errorString = tr("Failed to parse credentials %1").arg(error.errorString());
                Q_EMIT finished();
                return;
            }
            _data = obj.toVariant();
            OC_ASSERT(_data.isValid());
        } else {
            qCWarning(lcCredentialsManager) << u"Failed to get password" << scopedKey(_parent, _key) << _job->errorString();
            _error = _job->error();
            _errorString = _job->errorString();
        }
        Q_EMIT finished();
    });
    _job->start();
}

QString CredentialJob::key() const
{
    return _key;
}
