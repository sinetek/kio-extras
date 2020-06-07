/*
 * SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>

#include <KIO/SlaveBase>

#include <libimobiledevice/afc.h>
#include <libimobiledevice/libimobiledevice.h>

#include "afcdiskusage.h"
#include "afcfilereader.h"
#include "afcresult.h"

#include <KIO/UDSEntry>

class QDateTime;

class AfcDevice
{
public:
    explicit AfcDevice(const QString &id);
    ~AfcDevice();

    QString id() const;
    bool isValid() const;
    QString errorText() const;

    QString name() const;
    QString model() const;

    KIO::UDSEntry rootEntry(const QString &fileName = QString()) const;
    AfcResult entry(const QString &path, KIO::UDSEntry &entry) const;
    AfcResult entryList(const QString &path, QStringList &entryList) const;

    AfcResult open(const QString &path, QIODevice::OpenMode mode);
    AfcResult seek(KIO::filesize_t offset);
    AfcResult truncate(KIO::filesize_t length);
    AfcResult write(const QByteArray &data, uint32_t &bytesWritten);
    AfcResult close();

    AfcResult del(const QString &path);
    AfcResult rename(const QString &src, const QString &dest, KIO::JobFlags flags);
    AfcResult symlink(const QString &target, const QString &dest, KIO::JobFlags flags);
    AfcResult mkdir(const QString &path);
    AfcResult setModificationTime(const QString &path, const QDateTime &mtime);

    AfcDiskUsage diskUsage() const;
    AfcFileReader fileReader() const;

private:
    idevice_t m_device = nullptr;
    lockdownd_client_t m_lockdowndClient = nullptr;
    afc_client_t m_afcClient = nullptr;

    QString m_id;
    QString m_name;
    QString m_model;

    uint64_t m_currentHandle = static_cast<uint64_t>(-1);

};
