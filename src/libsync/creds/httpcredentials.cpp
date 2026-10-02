/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */
#include "creds/httpcredentials.h"

#include "accessmanager.h"
#include "account.h"
#include "configfile.h"
#include "creds/credentialmanager.h"
#include "oauth.h"
#include "syncengine.h"

#include <QAuthenticator>
#include <QLoggingCategory>
#include <QNetworkReply>


Q_LOGGING_CATEGORY(lcHttpCredentials, "sync.credentials.http", QtInfoMsg)

namespace {
auto refreshTokenKeyC()
{
    return QStringLiteral("http/oauthtoken");
}
}

namespace OCC {

class HttpCredentialsAccessManager : public AccessManager
{
    Q_OBJECT
public:
    HttpCredentialsAccessManager(const HttpCredentials *cred, QObject *parent = nullptr)
        : AccessManager(parent)
        , _cred(cred)
    {
    }

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) override
    {
        QNetworkRequest req(request);
        if (!req.attribute(HttpCredentials::DontAddCredentialsAttribute).toBool()) {
            if (_cred && _cred->ready() && !_cred->_accessToken.isEmpty()) {
                req.setRawHeader("Authorization", "Bearer " + _cred->_accessToken.toUtf8());
            }
        }
        return AccessManager::createRequest(op, req, outgoingData);
    }

private:
    // The credentials object dies along with the account, while the QNAM might
    // outlive both.
    QPointer<const HttpCredentials> _cred;
};

HttpCredentials::HttpCredentials(const QString &accessToken)
    : _accessToken(accessToken)
    , _ready(true)
{
}

AccessManager *HttpCredentials::createAM() const
{
    AccessManager *am = new HttpCredentialsAccessManager(this);

    connect(am, &QNetworkAccessManager::authenticationRequired,
        this, &HttpCredentials::slotAuthentication);

    return am;
}

bool HttpCredentials::ready() const
{
    return _ready;
}

void HttpCredentials::fetchFromKeychain()
{
    _wasFetched = true;
    if (_persistenceJob || _oAuthJob) {
        return;
    }

    if (!_ready && !_refreshToken.isEmpty()) {
        // This happens if the credentials are still loaded from the keychain, bur we are called
        // here because the auth is invalid, so this means we simply need to refresh the credentials
        refreshAccessToken();
        return;
    }

    if (_ready) {
        Q_EMIT fetched();
    } else {
        fetchFromKeychainHelper();
    }
}

void HttpCredentials::fetchFromKeychainHelper()
{
    auto job = _account->credentialManager()->get(refreshTokenKeyC());
    const auto generation = _credentialGeneration;
    connect(job, &CredentialJob::finished, this, [job, generation, this] {
        if (generation != _credentialGeneration) {
            return;
        }
        auto handleError = [job, this] {
            qCWarning(lcHttpCredentials) << u"Could not retrieve client password from keychain" << job->errorString();

            // we come here if the password is empty or any other keychain
            // error happend.

            _fetchErrorString = job->error() != QKeychain::EntryNotFound ? job->errorString() : QString();

            _accessToken.clear();
            _ready = false;
            Q_EMIT fetched();
        };
        if (job->error() != QKeychain::NoError) {
            handleError();
            return;
        }
        const auto data = job->data().toString();
        if (OC_ENSURE(!data.isEmpty())) {
            _refreshToken = data;
            refreshAccessToken();
        } else {
            handleError();
        }
    });
}

void HttpCredentials::checkCredentials(QNetworkReply *reply)
{
    // The function is called in order to determine whether we need to ask the user for a password
    // if we are using OAuth, we already started a refresh in slotAuthentication, at least in theory, ensure the auth is started.
    // If the refresh fails, we are going to Q_EMIT authenticationFailed ourselves
    if (reply->error() == QNetworkReply::AuthenticationRequiredError) {
        slotAuthentication(reply, nullptr);
    }
}

void HttpCredentials::slotAuthentication(QNetworkReply *reply, QAuthenticator *authenticator)
{
    qCDebug(lcHttpCredentials) << Q_FUNC_INFO << reply;
    if (!_ready)
        return;
    Q_UNUSED(authenticator)
    // Because of issue #4326, we need to set the login and password manually at every requests
    // Thus, if we reach this signal, those credentials were invalid and we terminate.
    qCWarning(lcHttpCredentials) << u"Stop request: Authentication failed for " << reply->url().toString() << reply->request().rawHeader("Original-Request-ID");

    if (!_oAuthJob) {
        qCInfo(lcHttpCredentials) << u"Refreshing token";
        refreshAccessToken();
    }
}

bool HttpCredentials::refreshAccessToken()
{
    _wasFetched = true;
    if (_oAuthJob || _persistenceJob) {
        return true;
    }
    if (_refreshToken.isEmpty()) {
        return false;
    }
    if (!_account->credentialManager()->beginRotation(refreshTokenKeyC())) {
        _accessToken.clear();
        _refreshToken.clear();
        _ready = false;
        Q_EMIT requestLogout();
        Q_EMIT fetched();
        return false;
    }
    const auto generation = ++_credentialGeneration;
    _ready = false;

    // parent with nam to ensure we reset when the nam is reset
    _oAuthJob = new AccountBasedOAuth(_account->sharedFromThis(), _account->accessManager());
    const auto oauth = _oAuthJob;
    connect(oauth, &AccountBasedOAuth::refreshError, this, [oauth, generation, this](QNetworkReply::NetworkError, const QString &) {
        oauth->deleteLater();
        if (generation != _credentialGeneration) {
            return;
        }
        _oAuthJob.clear();
        // The server may have consumed a rotating token even when its reply was lost.
        // Retain the durable fence and require fresh browser authentication.
        _accessToken.clear();
        _refreshToken.clear();
        _ready = false;
        // Stop sync without clearing queued jobs or their dirty journal state.
        Q_EMIT requestLogout();
        Q_EMIT fetched();
    });

    connect(oauth, &AccountBasedOAuth::refreshFinished, this, [oauth, generation, this](const QString &accessToken, const QString &refreshToken) {
        oauth->deleteLater();
        if (generation != _credentialGeneration) {
            return;
        }
        _oAuthJob.clear();
        if (refreshToken.isEmpty() || accessToken.isEmpty()) {
            _refreshToken.clear();
            _accessToken.clear();
            Q_EMIT requestLogout();
            Q_EMIT fetched();
            return;
        }
        _refreshToken = refreshToken;
        _accessToken = accessToken;
        persist();
    });
    Q_EMIT authenticationStarted();
    _oAuthJob->refreshAuthentication(_refreshToken);

    return true;
}

void HttpCredentials::invalidateToken()
{
    qCWarning(lcHttpCredentials) << u"Invalidating the credentials";

    if (!_accessToken.isEmpty()) {
        _previousPassword = _accessToken;
    }
    _accessToken = QString();
    _ready = false;

    // clear the session cookie.
    _account->clearCookieJar();

    if (!_refreshToken.isEmpty()) {
        // Only invalidate the access_token (_password) but keep the _refreshToken in the keychain
        // (when coming from forgetSensitiveData, the _refreshToken is cleared)
        return;
    }

    _account->credentialManager()->clear();
    // let QNAM forget about the password
    // This needs to be done later in the event loop because we might be called (directly or
    // indirectly) from QNetworkAccessManagerPrivate::authenticationRequired, which itself
    // is a called from a BlockingQueuedConnection from the Qt HTTP thread. And clearing the
    // cache needs to synchronize again with the HTTP thread.
    QTimer::singleShot(0, _account, &Account::clearAMCache);
}

void HttpCredentials::forgetSensitiveData()
{
    ++_credentialGeneration;
    if (_oAuthJob) {
        _oAuthJob->disconnect(this);
        _oAuthJob->deleteLater();
        _oAuthJob.clear();
    }
    // need to be done before invalidateToken, so it actually deletes the refresh_token from the keychain
    _refreshToken.clear();

    invalidateToken();
    _previousPassword.clear();
}

void HttpCredentials::persist()
{
    if (_persistenceJob || _refreshToken.isEmpty()) {
        return;
    }
    _ready = false;
    const auto generation = _credentialGeneration;
    auto manager = _account->credentialManager();
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(manager, &CredentialManager::writeFinished, this, [this, generation, connection](QKeychain::Job *job, bool success) {
        if (job != _persistenceJob) {
            return;
        }
        disconnect(*connection);
        _persistenceJob.clear();
        if (generation != _credentialGeneration) {
            return;
        }
        _ready = success && !_accessToken.isEmpty();
        _wasFetched = true;
        if (!_ready) {
            _accessToken.clear();
            _refreshToken.clear();
            _fetchErrorString = tr("Credentials could not be saved securely. Sign in again after the keychain is available.");
            Q_EMIT requestLogout();
        }
        Q_EMIT credentialsStored(_ready);
        Q_EMIT fetched();
    });
    _persistenceJob = manager->set(refreshTokenKeyC(), _refreshToken);
    if (!_persistenceJob) {
        disconnect(*connection);
        _accessToken.clear();
        _refreshToken.clear();
        _wasFetched = true;
        Q_EMIT credentialsStored(false);
        Q_EMIT requestLogout();
        Q_EMIT fetched();
    }
}

} // namespace OCC

#include "httpcredentials.moc"
