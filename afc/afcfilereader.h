/*
 * SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <KIO/Global>

#include <libimobiledevice/afc.h>

#include "afcresult.h"

class AfcFileReader
{
public:
    ~AfcFileReader();

    KIO::filesize_t size() const;
    void setSize(KIO::filesize_t size);

    AfcResult read();
    bool hasMore() const;
    QByteArray data() const;

private:
    friend class AfcDevice;

    AfcFileReader(afc_client_t afcClient, uint64_t handle);

    afc_client_t m_afcClient;
    uint64_t m_handle;
    KIO::filesize_t m_size = 0;
    KIO::filesize_t m_remainingSize = 0;

    QByteArray m_data;

};
