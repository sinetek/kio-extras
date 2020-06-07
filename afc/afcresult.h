/*
 * SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QString>

#include <KIO/Global>

#include <libimobiledevice/afc.h>

class AfcResult
{
public:
    explicit AfcResult(afc_error_t afcError, const QString &errorText = QString());
    explicit AfcResult(KIO::Error kioError, const QString &errorText = QString());

    // to make it more explicit in code than a default constructed object
    static AfcResult success();

    KIO::Error errorCode() const;
    QString errorText() const;

    explicit operator bool() const;

private:
    AfcResult();

    KIO::Error m_errorCode = static_cast<KIO::Error>(0);
    QString m_errorText;

};

QDebug operator<<(QDebug debug, const AfcResult &error);
