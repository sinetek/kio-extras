/*
 * SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <plist/plist.h>

#include <KIO/Global>

class AfcDiskUsage
{
public:
    AfcDiskUsage();
    explicit AfcDiskUsage(plist_t plist);

    bool isValid() const;

    KIO::filesize_t totalDiskCapacity() const;
    KIO::filesize_t totalDataCapacity() const;
    KIO::filesize_t totalDataAvailable() const;
    // There's also CameraUsage and MobileApplicationUsage

private:
    bool m_valid = false;

    KIO::filesize_t m_totalDiskCapacity = 0;
    KIO::filesize_t m_totalDataCapacity = 0;
    KIO::filesize_t m_totalDataAvailable = 0;
};
