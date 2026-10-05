/*
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

#include <QAuthenticator>
#include <QNetworkCookie>
#include <QSslSocket>
#include <QUuid>

#include "accessmanager.h"
#include "common/utility.h"
#include "cookiejar.h"
#include "httplogger.h"
#include "theme.h"

#include <algorithm>

namespace OCC {

Q_LOGGING_CATEGORY(lcAccessManager, "sync.accessmanager", QtInfoMsg)

AccessManager::AccessManager(QObject *parent, const OAuthIdentityProfile *identity)
    : QNetworkAccessManager(parent)
{
    // A compiled identity always outranks an injected unbranded test descriptor.
    const auto compiled = Theme::instance()->oauthIdentityProfile();
    const auto *profile = compiled ? &*compiled : identity;
    if (profile) {
        _pinned = true;
        _pinnedDriveOrigin = profile->driveOrigin;
        _pinnedIssuerOrigin = QUrl(profile->issuer);
    }
    setCookieJar(new CookieJar);

    connect(this, &AccessManager::sslErrors, this, [this](QNetworkReply *reply, const QList<QSslError> &errors) {
        if (_pinned) {
            return;
        }
        auto filtered = errors;
        filtered.erase(std::remove_if(
                           filtered.begin(), filtered.end(), [this](const QSslError &e) {
                               return !_customTrustedCaCertificates.contains(e.certificate());
                           }),
            filtered.end());
        reply->ignoreSslErrors(filtered);
    });
}

QByteArray AccessManager::generateRequestId()
{
    return QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
}

QNetworkReply *AccessManager::createRequest(QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData)
{
    QNetworkRequest newRequest(request);
    if (_pinned) {
        const auto sameOrigin = [](const QUrl &url, const QUrl &origin) {
            return url.isValid() && url.scheme() == QStringLiteral("https") && origin.scheme() == QStringLiteral("https") && url.userInfo().isEmpty()
                && url.fragment().isEmpty() && url.host() == origin.host() && url.port(443) == origin.port(443);
        };
        if (!sameOrigin(request.url(), _pinnedDriveOrigin) && !sameOrigin(request.url(), _pinnedIssuerOrigin)) {
            // Qt reports an invalid-URL error asynchronously. No headers, cookies, body or socket escape.
            return QNetworkAccessManager::createRequest(op, QNetworkRequest{}, nullptr);
        }
        newRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    }
    newRequest.setRawHeader(QByteArrayLiteral("User-Agent"), Utility::userAgentString());

    // Some firewalls reject requests that have a "User-Agent" but no "Accept" header
    newRequest.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("*/*"));

    // Set the language, so messages from the server are localised correctly.
    newRequest.setRawHeader("Accept-Language", QLocale().name().toUtf8());

    // we don't follow redirects, if we receive one the ConnectionValidor is triggered
    // -> default to manual redirection
    if (newRequest.attribute(QNetworkRequest::RedirectPolicyAttribute).isNull()) {
        newRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    }

    QByteArray verb = newRequest.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();
    // For PROPFIND (assumed to be a WebDAV op), set xml/utf8 as content type/encoding
    // This needs extension
    if (verb == QByteArrayLiteral("PROPFIND")) {
        newRequest.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("text/xml; charset=utf-8"));
    }

    // Generate a new request id
    const QByteArray requestId = generateRequestId();
    newRequest.setRawHeader(QByteArrayLiteral("X-Request-ID"), requestId);
    const auto originalIdKey = QByteArrayLiteral("Original-Request-ID");
    if (!newRequest.hasRawHeader(originalIdKey)) {
        newRequest.setRawHeader(originalIdKey, requestId);
    }


    // Whether or not to use HTTP/2, the build time option WITH_HTTP2 is used as a default. See https://github.com/opencloud-eu/desktop/blob/main/CMakeLists.txt
    static const bool useHttp2 = [] {
        if (!qEnvironmentVariableIsSet("OPENCLOUD_USE_HTTP2")) {
            // return build time default value
            return static_cast<bool>(WITH_HTTP2);
        }
        return qEnvironmentVariableIntValue("OPENCLOUD_USE_HTTP2") == 1;
    }();

    // Qt's default value is true, if we explicitly set it to false, we don't want to use HTTP/2
    if (newRequest.attribute(QNetworkRequest::Http2AllowedAttribute).toBool()) {
        newRequest.setAttribute(QNetworkRequest::Http2AllowedAttribute, useHttp2);
    }

    // allow http pipelining
    newRequest.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);

    auto sslConfiguration = _pinned ? QSslConfiguration::defaultConfiguration() : newRequest.sslConfiguration();
    if (_pinned) {
        sslConfiguration.setPeerVerifyMode(QSslSocket::VerifyPeer);
    }

    sslConfiguration.setSslOption(QSsl::SslOptionDisableSessionTickets, false);
    sslConfiguration.setSslOption(QSsl::SslOptionDisableSessionSharing, false);
    sslConfiguration.setSslOption(QSsl::SslOptionDisableSessionPersistence, false);
    if (!_pinned && !_customTrustedCaCertificates.isEmpty()) {
        // for some reason, passing an empty list causes the default chain to be removed
        // this behavior does not match the documentation
        sslConfiguration.addCaCertificates({ _customTrustedCaCertificates.begin(), _customTrustedCaCertificates.end() });
    }
    newRequest.setSslConfiguration(sslConfiguration);

    const auto reply = QNetworkAccessManager::createRequest(op, newRequest, outgoingData);
    HttpLogger::logRequest(reply, op, outgoingData);
    return reply;
}

QSet<QSslCertificate> AccessManager::customTrustedCaCertificates()
{
    return _customTrustedCaCertificates;
}

void AccessManager::setCustomTrustedCaCertificates(const QSet<QSslCertificate> &certificates)
{
    _customTrustedCaCertificates = certificates;
    // we have to terminate the existing (cached) connection to make the access manager re-evaluate the certificate sent by the server
    clearConnectionCache();
}

void AccessManager::addCustomTrustedCaCertificates(const QList<QSslCertificate> &certificates)
{
    _customTrustedCaCertificates.unite({ certificates.begin(), certificates.end() });

    // we have to terminate the existing (cached) connection to make the access manager re-evaluate the certificate sent by the server
    clearConnectionCache();
}

CookieJar *AccessManager::openCloudCookieJar() const
{
    auto jar = qobject_cast<CookieJar *>(cookieJar());
    Q_ASSERT(jar);
    return jar;
}

QList<QSslError> AccessManager::filterSslErrors(const QList<QSslError> &errors) const
{
    if (_pinned) {
        return errors;
    }
    auto filtered = errors;
    filtered.erase(std::remove_if(filtered.begin(), filtered.end(),
                       [this](const QSslError &e) { return e.certificate().isNull() || _customTrustedCaCertificates.contains(e.certificate()); }),
        filtered.end());
    return filtered;
}

} // namespace OCC
