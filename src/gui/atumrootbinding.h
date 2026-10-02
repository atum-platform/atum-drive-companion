// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "gui/opencloudguilib.h"
#include "libsync/common/result.h"
#include <QJsonObject>

#include <QString>
#include <memory>

namespace OCC {

// Non-secret identity facts. These never confer authentication authority.
struct OPENCLOUD_GUI_EXPORT AtumRootIdentity
{
    QString origin;
    QString issuer;
    QString subject;
    QString space;
};

class OPENCLOUD_GUI_EXPORT AtumRootBinding
{
public:
    static QString journalName();
    static Result<void, QString> checkCandidate(const QString &path, const QString &issuer, const QString &subject);
    static Result<std::unique_ptr<AtumRootBinding>, QString> acquire(const QString &path, const AtumRootIdentity &identity, bool enrollEmpty);
    ~AtumRootBinding();
    bool matches(const AtumRootIdentity &identity) const;
    QString canonicalRoot() const;

private:
    AtumRootBinding(const QString &root, const QJsonObject &binding);
    bool lockOwner();
    QString _root;
    QJsonObject _binding;
    qintptr _directoryHandle = -1;
    qintptr _ownerHandle = -1;
};
}
