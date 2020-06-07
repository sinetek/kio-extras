/*
 * SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "afcurl.h"

AfcUrl::AfcUrl(const QUrl &url)
{
    if (!url.isValid() || url.scheme() != QLatin1String("afc")) {
        return;
    }

    const QString path = url.path();
    if (path.length() <= 1) {
        // too short to have any device name
        return;
    }

    Q_ASSERT(path.startsWith(QLatin1Char('/')));

    const int slashIdx = path.indexOf(QLatin1Char('/'), 1);
    if (slashIdx == -1) {
        m_deviceId = path.mid(1);
        return;
    }

    m_deviceId = path.mid(1, slashIdx - 1);
    m_path = path.mid(slashIdx + 1);
}

QString AfcUrl::deviceId() const
{
    return m_deviceId;
}

void AfcUrl::setDeviceId(const QString &deviceId)
{
    m_deviceId = deviceId;
}

QString AfcUrl::path() const
{
    return m_path;
}

void AfcUrl::setPath(const QString &path)
{
    m_path = path;
}
