/*
 * SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "afcfilereader.h"

#include "afc-debug.h"

#include <limits>

using namespace KIO;

AfcFileReader::AfcFileReader(afc_client_t afcClient, uint64_t handle)
    : m_afcClient(afcClient)
    , m_handle(handle)
{

}

AfcFileReader::~AfcFileReader()
{

}

filesize_t AfcFileReader::size() const
{
    return m_size;
}

void AfcFileReader::setSize(filesize_t size)
{
    m_size = size;
    m_remainingSize = size;
    m_data.clear();
}

AfcResult AfcFileReader::read()
{
    m_data.clear();

    if (m_remainingSize == 0) {
        return AfcResult::success();
    }

    int bytesToRead = std::numeric_limits<int>::max();
    if (m_remainingSize < static_cast<KIO::filesize_t>(bytesToRead)) {
        bytesToRead = m_remainingSize;
    }

    // FIXME length is always zero because we bluntly write into it?
    if (m_data.length() < bytesToRead) {
        // TODO catch std::bad_alloc?
        m_data.resize(bytesToRead);
    }

    uint32_t bytesRead = 0;
    afc_error_t ret = AFC_E_SUCCESS;

    ret = afc_file_read(m_afcClient, m_handle, m_data.data(), bytesToRead, &bytesRead);
    m_data.resize(bytesRead);

    if (ret != AFC_E_SUCCESS && ret != AFC_E_END_OF_DATA) {
        return AfcResult(ret);
    }

    m_remainingSize -= bytesRead;
    return AfcResult::success();
}

QByteArray AfcFileReader::data() const
{
    return m_data;
}

bool AfcFileReader::hasMore() const
{
    return m_remainingSize > 0;
}
