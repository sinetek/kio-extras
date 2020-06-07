/*
 * SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "afcdiskusage.h"

using namespace KIO;

AfcDiskUsage::AfcDiskUsage() = default;

AfcDiskUsage::AfcDiskUsage(plist_t plist)
{
    uint64_t value = 0;
    if (auto item = plist_dict_get_item(plist, "TotalDiskCapacity")) {
        plist_get_uint_val(item, &value);
        m_totalDiskCapacity = value;
    } else {
        return;
    }

    if (auto item = plist_dict_get_item(plist, "TotalDataCapacity")) {
        plist_get_uint_val(item, &value);
        m_totalDataCapacity = value;
    } else {
        return;
    }

    if (auto item = plist_dict_get_item(plist, "TotalDataAvailable")) {
        plist_get_uint_val(item, &value);
        m_totalDataAvailable = value;
    } else {
        return;
    }

    m_valid = true;
}

bool AfcDiskUsage::isValid() const
{
    return m_valid;
}

filesize_t AfcDiskUsage::totalDiskCapacity() const
{
    return m_totalDiskCapacity;
}

filesize_t AfcDiskUsage::totalDataCapacity() const
{
    return m_totalDataCapacity;
}

filesize_t AfcDiskUsage::totalDataAvailable() const
{
    return m_totalDataAvailable;
}
