/*
 * Copyright (C) Fabian Müller <fmueller@owncloud.com>
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

#pragma once

#include "abstractcorejob.h"
#include "gui/opencloudguilib.h"
#include "theme.h"

namespace OCC::Wizard::Jobs {

class OPENCLOUD_GUI_EXPORT ResolveUrlJobFactory : public AbstractCoreJobFactory
{
public:
    explicit ResolveUrlJobFactory(QNetworkAccessManager *nam, std::optional<OAuthIdentityProfile> identity = std::nullopt);

    CoreJob *startJob(const QUrl &url, QObject *parent) override;

private:
    const std::optional<OAuthIdentityProfile> _identity;
};
}
