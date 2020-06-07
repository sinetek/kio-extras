/*
 * SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "afcresult.h"

#include "afc-debug.h"

#include <QDebug>

AfcResult::AfcResult()
{

}

AfcResult::AfcResult(afc_error_t afcError, const QString &errorText)
{
    switch (afcError) {
    case AFC_E_SUCCESS:
    case AFC_E_END_OF_DATA:
        m_errorCode = static_cast<KIO::Error>(0); // KJob::NoError
        break;

    case AFC_E_UNKNOWN_ERROR:
        m_errorCode = KIO::ERR_UNKNOWN;
        break;

    case AFC_E_NO_RESOURCES:
    case AFC_E_NO_MEM:
        m_errorCode = KIO::ERR_OUT_OF_MEMORY;
        break;

    case AFC_E_READ_ERROR:
        m_errorCode = KIO::ERR_COULD_NOT_READ;
        break;
    case AFC_E_WRITE_ERROR:
        m_errorCode = KIO::ERR_COULD_NOT_WRITE;
        break;
    case AFC_E_OBJECT_NOT_FOUND:
        m_errorCode = KIO::ERR_DOES_NOT_EXIST;
        break;
    case AFC_E_OBJECT_IS_DIR:
        m_errorCode = KIO::ERR_IS_DIRECTORY;
        break;
    case AFC_E_PERM_DENIED:
        m_errorCode = KIO::ERR_ACCESS_DENIED;
        break;
    case AFC_E_SERVICE_NOT_CONNECTED :
        m_errorCode = KIO::ERR_CONNECTION_BROKEN;
        break;
    case AFC_E_OP_TIMEOUT:
        m_errorCode = KIO::ERR_SERVER_TIMEOUT;
        break;
    case AFC_E_OP_NOT_SUPPORTED:
        m_errorCode = KIO::ERR_UNSUPPORTED_ACTION;
        break;
    case AFC_E_OBJECT_EXISTS:
        m_errorCode = KIO::ERR_FILE_ALREADY_EXIST;
        break;
    case AFC_E_NO_SPACE_LEFT:
        m_errorCode = KIO::ERR_DISK_FULL;
        break;
    case AFC_E_IO_ERROR:
        m_errorCode = KIO::ERR_CONNECTION_BROKEN;
        break;
    case AFC_E_INTERNAL_ERROR:
        m_errorCode = KIO::ERR_INTERNAL_SERVER;
        break;
    case AFC_E_DIR_NOT_EMPTY:
        m_errorCode = KIO::ERR_COULD_NOT_RMDIR;
        break;

    case AFC_E_OP_HEADER_INVALID:
    case AFC_E_UNKNOWN_PACKET_TYPE:
    case AFC_E_INVALID_ARG:
    case AFC_E_TOO_MUCH_DATA:
    case AFC_E_OBJECT_BUSY:
    case AFC_E_FORCE_SIGNED_TYPE:
    case AFC_E_OP_INTERRUPTED: // UNKNOWN_INTERRUPT?
    case AFC_E_OP_IN_PROGRESS:
    case AFC_E_OP_WOULD_BLOCK:
    case AFC_E_MUX_ERROR:
    case AFC_E_NOT_ENOUGH_DATA:
        m_errorCode = KIO::ERR_INTERNAL;
        break;
    default:
        qCWarning(KIO_AFC_LOG) << "Unhandled afc_error_t" << afcError;
    }

    if (m_errorCode) {
        m_errorText = errorText;
    }
}

AfcResult::AfcResult(KIO::Error kioError, const QString &errorText)
    : m_errorCode(kioError)
    , m_errorText(errorText)
{

}

AfcResult AfcResult::success()
{
    return AfcResult();
}

KIO::Error AfcResult::errorCode() const
{
    return m_errorCode;
}

QString AfcResult::errorText() const
{
    return m_errorText;
}

AfcResult::operator bool() const
{
    return m_errorCode == 0;
}

QDebug operator<<(QDebug debug, const AfcResult &result)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "AfcResult(";
    if (result) {
        debug << "[Success]";
    } else {
        debug << result.errorCode();
    }
    debug << ")";
    return debug;
}
