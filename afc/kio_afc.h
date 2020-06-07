/*
 * SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <KIO/SlaveBase>

#include <libimobiledevice/libimobiledevice.h>

#include <QHash>
#include <QString>

#include "afcurl.h"

using namespace KIO;

class AfcDevice;
class AfcResult;

class AfcSlave : public QObject, public KIO::SlaveBase
{
    Q_OBJECT

public:
    AfcSlave(const QByteArray &pool, const QByteArray &app);
    ~AfcSlave() override;

    void onDeviceEvent(const idevice_event_t *event);

    void listDir(const QUrl &url) override;

    void stat(const QUrl &url) override;

    void get(const QUrl &url) override;
    void put(const QUrl &url, int permissions, KIO::JobFlags flags) override;

    void open(const QUrl &url, QIODevice::OpenMode mode) override;
    void read(KIO::filesize_t bytesRequested) override;
    void seek(KIO::filesize_t offset) override;
    void write(const QByteArray &data) override;
    void close() override;

    void del(const QUrl &url, bool isFile) override;
    // direct copy not supported by afc
    void rename(const QUrl &url, const QUrl &dest, KIO::JobFlags flags) override;
    void symlink(const QString &target, const QUrl &dest, KIO::JobFlags flags) override;
    void mkdir(const QUrl &url, int permissions) override;
    void setModificationTime(const QUrl &url, const QDateTime &mtime) override;

    void fileSystemFreeSpace(const QUrl &url);
    void cotruncate(KIO::filesize_t length);

    void virtual_hook(int id, void *data) override;

private:
    AfcUrl afcUrl(const QUrl &url) const;
    void emitResult(const AfcResult &result);

    void updateDeviceList(bool *foundInvalidDevices = nullptr);
    bool addDevice(const QString &id);
    void removeDevice(const QString &id);

    QUrl resolveSolidUrl(const QUrl &url) const;
    bool redirectIfSolidUrl(const QUrl &url);

    void guessMimeType(AfcDevice *device, const QString &path);

    QHash<QString, AfcDevice *> m_devices;
    QHash<QString /*unique pretty name*/, QString /*id*/> m_deviceUniqueNames;

    AfcDevice *m_openDevice = nullptr;

};
