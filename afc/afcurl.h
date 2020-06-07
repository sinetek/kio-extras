/*
 * SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QString>
#include <QUrl>

class AfcUrl
{
public:
    explicit AfcUrl(const QUrl &url);

    QString deviceId() const;
    void setDeviceId(const QString &deviceId);

    QString path() const;
    void setPath(const QString &path);

private:
    QString m_deviceId;
    QString m_path;

};
