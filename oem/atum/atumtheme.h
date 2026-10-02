// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "atumprofile.h"
#include "opencloudtheme.h"

namespace OCC {
class AtumTheme : public OpenCloudTheme
{
public:
    QString oauthClientId() const override { return QStringLiteral(ATUM_OAUTH_CLIENT_ID); }
    bool oidcEnableDynamicRegistration() const override { return false; }
    std::optional<OAuthIdentityProfile> oauthIdentityProfile() const override
    {
        return OAuthIdentityProfile{
            QUrl(QStringLiteral(ATUM_DRIVE_ORIGIN)), QStringLiteral(ATUM_MAS_ISSUER), QStringLiteral(ATUM_OAUTH_CLIENT_ID), QStringLiteral("openid")};
    }
    QColor wizardHeaderBackgroundColor() const override { return QColor(QStringLiteral("#142638")); }
    QmlButtonColor primaryButtonColor() const override { return {"#8DE4D3", "#142638", "#DADADA"}; }
    QUrl helpUrl() const override { return QUrl(QStringLiteral("https://github.com/atum-platform/atum-drive-companion")); }
};
}
