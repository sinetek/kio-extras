/*
 * SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "afcdevice.h"

#include "afc-debug.h"

#include "afcresult.h"

#include <QDateTime>
#include <QScopeGuard>

#include <libimobiledevice/afc.h>
#include <libimobiledevice/lockdown.h>

using namespace KIO;

AfcDevice::AfcDevice(const QString &id)
    : m_id(id)
{
    idevice_new(&m_device, id.toUtf8().constData());
    if (!m_device) {
        qCWarning(KIO_AFC_LOG) << "Failed to create idevice for" << id;
        return;
    }

    auto ret = lockdownd_client_new_with_handshake(m_device, &m_lockdowndClient, "kio_afc");
    if (ret != LOCKDOWN_E_SUCCESS) {
        qCWarning(KIO_AFC_LOG) << "Failed to create lockdown client for" << id << ret << "make sure the device is unlocked and trusted";
        return;
    }

    lockdownd_service_descriptor_t service = nullptr;
    ret = lockdownd_start_service(m_lockdowndClient,  "com.apple.afc", &service);
    if (ret != LOCKDOWN_E_SUCCESS) {
        qCWarning(KIO_AFC_LOG) << "Failed to start AFC service through lockdownd on" << id;
        return;
    }

    auto afcRet = afc_client_new(m_device, service, &m_afcClient);
    if (afcRet != AFC_E_SUCCESS) {
        qCWarning(KIO_AFC_LOG) << "Failed to create AFC client for" << id;
        return;
    }

    char *name = nullptr;
    auto lockdownRet = lockdownd_get_device_name(m_lockdowndClient, &name);
    if (lockdownRet != LOCKDOWN_E_SUCCESS) {
        qCWarning(KIO_AFC_LOG) << "Failed to get device name for" << id;
    } else {
        m_name = QString::fromUtf8(name);
        free(name);
    }

    char *model = nullptr;
    if (afc_get_device_info_key(m_afcClient, "Model", &model) != AFC_E_SUCCESS) {
        qCWarning(KIO_AFC_LOG) << "Failed to get device model for" << id;
    } else {
        m_model = QString::fromUtf8(model);
        free(model);
    }
}

AfcDevice::~AfcDevice()
{
    if (m_afcClient) {
        afc_client_free(m_afcClient);
        m_afcClient = nullptr;
    }

    if (m_lockdowndClient) {
        lockdownd_client_free(m_lockdowndClient);
        m_lockdowndClient = nullptr;
    }

    if (m_device) {
        idevice_free(m_device);
        m_device = nullptr;
    }
}

QString AfcDevice::id() const
{
    return m_id;
}

bool AfcDevice::isValid() const
{
    return m_device && m_afcClient;
}

QString AfcDevice::name() const
{
    return m_name;
}

QString AfcDevice::model() const
{
    return m_model;
}

UDSEntry AfcDevice::rootEntry(const QString &fileName) const
{
    UDSEntry entry;
    entry.fastInsert(UDSEntry::UDS_NAME, !fileName.isEmpty() ? fileName : m_id);
    entry.fastInsert(UDSEntry::UDS_DISPLAY_NAME, m_name);
    // TODO prettier
    entry.fastInsert(UDSEntry::UDS_DISPLAY_TYPE, m_model);
    entry.fastInsert(UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.fastInsert(UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));

    QString iconName;
    if (m_model.contains(QLatin1String("iPhone"))) {
        iconName = QStringLiteral("phone-apple-iphone");
    } else if (m_model.contains(QLatin1String("iPad"))) {
        iconName = QStringLiteral("computer-apple-ipad");
    } else if (m_model.contains(QLatin1String("iPod"))) {
       // We can assume iPod running iOS/supporting imobiledevice is an iPod touch?
       iconName = QStringLiteral("multimedia-player-apple-ipod-touch");
    }

    if (!iconName.isEmpty()) {
        entry.fastInsert(UDSEntry::UDS_ICON_NAME, iconName);
    }

    return entry;
}

AfcResult AfcDevice::entry(const QString &path, UDSEntry &entry) const
{
    char **info = nullptr;
    const auto ret = afc_get_file_info(m_afcClient, path.toUtf8(), &info);
    // may return null https://github.com/libimobiledevice/libimobiledevice/issues/206
    const auto result = AfcResult(ret, path);
    if (!result || !info) {
        return result;
    }

    auto cleanup = qScopeGuard([&info] {
        afc_dictionary_free(info);
    });

    const int lastSlashIdx = path.lastIndexOf(QLatin1Char('/'));
    entry.fastInsert(UDSEntry::UDS_NAME, path.mid(lastSlashIdx + 1));

    // Apply special icons for known locations
    static const QHash<QString, QString> s_folderIcons = {
        {QStringLiteral("/DCIM"), QStringLiteral("camera-photo")},
        {QStringLiteral("/Downloads"), QStringLiteral("folder-downloads")},
        {QStringLiteral("/Photos"), QStringLiteral("folder-pictures")}
    };
    const QString iconName = s_folderIcons.value(path);
    if (!iconName.isEmpty()) {
        entry.fastInsert(UDSEntry::UDS_ICON_NAME, iconName);
    }

    for (int i = 0; info[i]; i += 2) {
        const auto *key = info[i];
        const auto *value = info[i + 1];

        if (strcmp(key, "st_size") == 0) {
            entry.fastInsert(UDSEntry::UDS_SIZE, atoll(value));
        } else if (strcmp(key, "st_blocks") == 0) {

        } else if (strcmp(key, "st_nlink") == 0) {

        } else if (strcmp(key, "st_ifmt") == 0) {
            int type = 0;
            if (strcmp(value, "S_IFREG") == 0) {
                type = S_IFREG;
            } else if (strcmp(value, "S_IFDIR") == 0) {
                // TODO also inode/directory mime?
                type = S_IFDIR;
            } else if (strcmp(value, "S_IFLNK") == 0) {
                type = S_IFLNK;
            }
            // TODO S_IFMT, S_IFCHR, S_IFBLK, S_IFIFO, S_IFSOCK?

            if (type) {
                entry.fastInsert(UDSEntry::UDS_FILE_TYPE, type);
            } else {
                qCWarning(KIO_AFC_LOG) << "Encountered unknown" << key << "of" << value << "for" << path;
            }
        // is returned in nanoseconds
        } else if (strcmp(key, "st_mtime") == 0) {
            entry.fastInsert(UDSEntry::UDS_MODIFICATION_TIME, atoll(value) / 1000000000);
        } else if (strcmp(key, "st_birthtime") == 0) {
            entry.fastInsert(UDSEntry::UDS_CREATION_TIME, atoll(value) / 1000000000);
        } else {
            qCDebug(KIO_AFC_LOG) << "Encountered unrecognized file info key" << key << "for" << path;
        }
    }

    return AfcResult::success();
}

AfcResult AfcDevice::entryList(const QString &path, QStringList &entryList) const
{
    char **entries = nullptr;
    entryList.clear();

    const auto ret = afc_read_directory(m_afcClient, path.toUtf8(), &entries);
    const auto result = AfcResult(ret);
    if (!result || !entries) {
        return result;
    }

    auto cleanup = qScopeGuard([&entries] {
        afc_dictionary_free(entries);
    });

    char **it = entries;
    while (*it) {
        const QString name = QString::fromUtf8(*it);
        ++it;

        if (name == QLatin1Char('.') || name == QLatin1String("..")) {
            continue;
        }
        entryList.append(name);
    }

    return AfcResult::success();
}

AfcResult AfcDevice::open(const QString &path, QIODevice::OpenMode mode)
{
    Q_ASSERT(m_currentHandle == static_cast<uint64_t>(-1));

    afc_file_mode_t fileMode = static_cast<afc_file_mode_t>(0);

    if (mode == QIODevice::ReadOnly) {
        fileMode = AFC_FOPEN_RDONLY;
    } else if (mode == QIODevice::WriteOnly) {
        fileMode = AFC_FOPEN_WRONLY;
    } else if (mode == QIODevice::ReadWrite) {
        fileMode = AFC_FOPEN_RW;
    } else if (mode == (QIODevice::ReadWrite | QIODevice::Truncate)) {
        fileMode = AFC_FOPEN_WR;
    } else if (mode == QIODevice::Append || mode == (QIODevice::Append | QIODevice::WriteOnly)) {
        fileMode = AFC_FOPEN_APPEND;
    } else if (mode == (QIODevice::Append | QIODevice::ReadWrite)) {
        fileMode = AFC_FOPEN_RDAPPEND;
    }

    if (!fileMode) {
        return AfcResult(KIO::ERR_UNSUPPORTED_ACTION, QString::number(mode));
    }

    const auto ret = afc_file_open(m_afcClient, path.toLocal8Bit().constData(), fileMode, &m_currentHandle);
    return AfcResult(ret, path);
}

AfcResult AfcDevice::seek(filesize_t offset)
{
    Q_ASSERT(m_currentHandle != static_cast<uint64_t>(-1));
    const auto ret = afc_file_seek(m_afcClient, m_currentHandle, offset, SEEK_SET);
    return AfcResult(ret);
}

AfcResult AfcDevice::truncate(filesize_t length)
{
    Q_ASSERT(m_currentHandle != static_cast<uint64_t>(-1));
    const auto ret = afc_file_truncate(m_afcClient, m_currentHandle, length);
    return AfcResult(ret);
}

AfcResult AfcDevice::write(const QByteArray &data, uint32_t &bytesWritten)
{
    Q_ASSERT(m_currentHandle != static_cast<uint64_t>(-1));
    const auto ret = afc_file_write(m_afcClient, m_currentHandle, data.constData(), data.size(), &bytesWritten);
    return AfcResult(ret);
}

AfcResult AfcDevice::close()
{
    Q_ASSERT(m_currentHandle != static_cast<uint64_t>(-1));

    const auto ret = afc_file_close(m_afcClient, m_currentHandle);

    const auto result = AfcResult(ret);
    if (!result) {
        return result;
    }

    m_currentHandle = -1;
    return result;
}

AfcResult AfcDevice::del(const QString &path)
{
    const auto ret = afc_remove_path(m_afcClient, path.toUtf8());
    return AfcResult(ret, path);
}

AfcResult AfcDevice::rename(const QString &src, const QString &dest, JobFlags flags)
{
    UDSEntry srcEntry; // unused
    const auto srcResult = this->entry(src, srcEntry);
    if (!srcResult) {
        return srcResult;
    }

    UDSEntry destEntry;
    const auto destResult = this->entry(dest, destEntry);

    const bool exists = destResult.errorCode() != ERR_DOES_NOT_EXIST;
    if (exists && !flags.testFlag(KIO::Overwrite)) {
        if (S_ISDIR(destEntry.numberValue(UDSEntry::UDS_FILE_TYPE))) {
            return AfcResult(ERR_DIR_ALREADY_EXIST, dest);
        }
        return AfcResult(ERR_FILE_ALREADY_EXIST, dest);
    }

    const auto ret = afc_rename_path(m_afcClient,
                                     src.toUtf8().constData(),
                                     dest.toUtf8().constData());
    return AfcResult(ret, dest);
}

AfcResult AfcDevice::symlink(const QString &target, const QString &dest, JobFlags flags)
{
    UDSEntry targetEntry; // unused
    const auto targetResult = this->entry(target, targetEntry);
    if (!targetResult) {
        return targetResult;
    }

    UDSEntry destEntry;
    const auto destResult = this->entry(dest, destEntry);

    const bool exists = destResult.errorCode() != ERR_DOES_NOT_EXIST;
    if (exists && !flags.testFlag(KIO::Overwrite)) {
        if (S_ISDIR(destEntry.numberValue(UDSEntry::UDS_FILE_TYPE))) {
            return AfcResult(ERR_DIR_ALREADY_EXIST, dest);
        }
        return AfcResult(ERR_FILE_ALREADY_EXIST, dest);
    }

    const auto ret = afc_make_link(m_afcClient,
                                   AFC_SYMLINK,
                                   target.toUtf8().constData(),
                                   dest.toUtf8().constData());
    return AfcResult(ret, dest);
}

AfcResult AfcDevice::mkdir(const QString &path)
{
    UDSEntry entry;
    const auto getResult = this->entry(path, entry);

    const bool exists = getResult.errorCode() != ERR_DOES_NOT_EXIST;
    if (exists) {
        if (S_ISDIR(entry.numberValue(UDSEntry::UDS_FILE_TYPE))) {
            return AfcResult(ERR_DIR_ALREADY_EXIST, path);
        }
        return AfcResult(ERR_FILE_ALREADY_EXIST, path);
    }

    const auto ret = afc_make_directory(m_afcClient, path.toUtf8().constData());
    return AfcResult(ret, path);
}

AfcResult AfcDevice::setModificationTime(const QString &path, const QDateTime &mtime)
{
    const auto ret = afc_set_file_time(m_afcClient,
                                       path.toUtf8().constData(),
                                       mtime.toMSecsSinceEpoch() /*ms*/ * 1000000 /*us*/);
    return AfcResult(ret, path);
}

AfcDiskUsage AfcDevice::diskUsage() const
{
    plist_t plist = nullptr;
    if (lockdownd_get_value(m_lockdowndClient,
                            "com.apple.disk_usage",
                            nullptr /*key*/,
                            &plist) != LOCKDOWN_E_SUCCESS) {
        return AfcDiskUsage();
    }

    AfcDiskUsage usage(plist);

    plist_free(plist);
    return usage;
}

AfcFileReader AfcDevice::fileReader() const
{
    Q_ASSERT(m_currentHandle != static_cast<uint64_t>(-1));
    AfcFileReader reader(m_afcClient, m_currentHandle);
    return reader;
}
