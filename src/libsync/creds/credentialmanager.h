#pragma once

#include <QHash>
#include <QSet>
#include <QSettings>
#include <QVariant>

#include "opencloudsynclib.h"

#include <qt6keychain/keychain.h>

#include <functional>
#include <memory>

namespace OCC {
class Account;
class CredentialJob;

class OPENCLOUD_SYNC_EXPORT CredentialManager : public QObject
{
    Q_OBJECT
public:
    // global credentials
    CredentialManager(QObject *parent);
    // account related credentials
    explicit CredentialManager(Account *acc);

    CredentialJob *get(const QString &key);
    QKeychain::Job *set(const QString &key, const QVariant &data);
    QKeychain::Job *remove(const QString &key);
    /**
     * Delete all credentials asigned with an account
     */
    QVector<QPointer<QKeychain::Job>> clear(const QString &group = {});

    bool contains(const QString &key) const;
    const Account *account() const;

    // A rotation consumes the previous token. Persist this fence before sending it.
    bool beginRotation(const QString &key);
    bool hasPendingOperation(const QString &key) const;
    bool hasPendingDeletion() const;
    bool deletionFailed() const;

Q_SIGNALS:
    void stateChanged();
    void writeFinished(QKeychain::Job *job, bool success);

private:
    QSettings &credentialsList() const;
    QSettings &operations() const;
    QString binding() const;
    bool markPending(const QString &key, const QString &operation);
    QStringList pendingKeys() const;

    // TestCredentialManager
    QStringList knownKeys(const QString &group = {}) const;

    Account *const _account = nullptr;
    mutable std::unique_ptr<QSettings> _credentialsList;
    mutable std::unique_ptr<QSettings> _operations;
    mutable QString _binding;
    QHash<QString, quint64> _generations;
    QHash<QString, QPointer<QKeychain::Job>> _writes;
    QHash<QString, QPointer<QKeychain::Job>> _deletes;
    QSet<QString> _deleteAgain;
    // Controlled job scheduling for custody failure/order tests; production
    // always starts the real OS-backed QKeychain job without plaintext fallback.
    std::function<void(QKeychain::Job *)> _startJob = [](QKeychain::Job *job) { job->start(); };

    friend class TestCredentialManager;
    friend class CredentialJob;
};

class OPENCLOUD_SYNC_EXPORT CredentialJob : public QObject
{
    Q_OBJECT
public:
    QString key() const;

    QKeychain::Error error() const;

    const QVariant &data() const;

    QString errorString() const;

Q_SIGNALS:
    void finished();

private:
    CredentialJob(CredentialManager *parent, const QString &key);
    void start();

    QString _key;
    QVariant _data;
    QKeychain::Error _error = QKeychain::NoError;
    QString _errorString;
    bool _retryOnKeyChainError = true;
    QKeychain::ReadPasswordJob *_job = nullptr;

    CredentialManager *const _parent;

    friend class CredentialManager;
    friend class TestCredentialManager;
};


}
