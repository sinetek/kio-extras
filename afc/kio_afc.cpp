/*
 * SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "kio_afc.h"

#include "afc-debug.h"

#include "afcdevice.h"
#include "afcresult.h"
#include "afcfilereader.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QScopeGuard>
#include <QMimeDatabase>
#include <QMimeType>

#include <KLocalizedString>

using namespace KIO;

// Pseudo plugin class to embed meta data
class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.slave.afc" FILE "afc.json")
};

UDSEntry overviewEntry(const QString &name = QString())
{
    UDSEntry entry;
    entry.fastInsert(UDSEntry::UDS_NAME, !name.isEmpty() ? name : i18n("Apple Devices"));
    entry.fastInsert(UDSEntry::UDS_ICON_NAME, QStringLiteral("phone-apple-iphone"));
    return entry;
}

AfcSlave::AfcSlave(const QByteArray &pool, const QByteArray &app)
    : SlaveBase("afc", pool, app)
{
    idevice_event_subscribe([](const idevice_event_t *event, void *user_data) {
        static_cast<AfcSlave *>(user_data)->onDeviceEvent(event);
    }, this);

    updateDeviceList();
}

AfcSlave::~AfcSlave()
{
    idevice_event_unsubscribe();

    // Should we close it first?
    m_openDevice = nullptr;

    qDeleteAll(m_devices);
    m_devices.clear();
}

void AfcSlave::onDeviceEvent(const idevice_event_t *event)
{
    switch (event->event) {
    case IDEVICE_DEVICE_ADD:
        addDevice(QString::fromLatin1(event->udid));
        return;
    case IDEVICE_DEVICE_REMOVE: {
        removeDevice(QString::fromLatin1(event->udid));
        return;
    }
    case IDEVICE_DEVICE_PAIRED:
        return;
    }

    qCWarning(KIO_AFC_LOG) << "Unhandled idevice event" << event->event << "for" << event->udid;
}

AfcUrl AfcSlave::afcUrl(const QUrl &url) const
{
    AfcUrl afcUrl(url);
    // Resolve pretty URL
    const QString deviceIdFromUniqueName = m_deviceUniqueNames.value(afcUrl.deviceId());
    if (!deviceIdFromUniqueName.isEmpty()) {
        afcUrl.setDeviceId(deviceIdFromUniqueName);
    }
    return afcUrl;
}

void AfcSlave::emitResult(const AfcResult &result)
{
    if (result) {
        finished();
    } else {
        error(result.errorCode(), result.errorText());
    }
}

QUrl AfcSlave::resolveSolidUrl(const QUrl &url) const
{
    const QString path = url.path();

    const QString prefix = QStringLiteral("udi=/org/kde/solid/imobile/");
    if (!path.startsWith(prefix)) {
        return {};
    }

    QString deviceId = path.mid(prefix.length());
    const int slashIdx = deviceId.indexOf(QLatin1Char('/'));
    if (slashIdx > -1) {
        deviceId = deviceId.left(slashIdx);
    }

    QString newPath = m_deviceUniqueNames.key(deviceId);
    if (newPath.isEmpty()) {
        newPath = deviceId;
    }

    // TODO would be nice to preserve subdirectories
    const QUrl newUrl(QStringLiteral("afc:/%1").arg(newPath));
    return newUrl;
}

bool AfcSlave::redirectIfSolidUrl(const QUrl &url)
{
    const QUrl redirectUrl = resolveSolidUrl(url);
    if (!redirectUrl.isValid()) {
        return false;
    }

    redirection(redirectUrl);
    finished();
    return true;
}

void AfcSlave::updateDeviceList(bool *foundInvalidDevices)
{
    char **devices = nullptr;
    int count = 0;

    idevice_get_device_list(&devices, &count);
    for (int i = 0; i < count; ++i) {
        const QString id = QString::fromLatin1(devices[i]);
        if (!addDevice(id)) {
            if (foundInvalidDevices) {
                *foundInvalidDevices = true;
                // don't break, we still want to find all devices
            }
        }
    }

    if (devices) {
        idevice_device_list_free(devices);
    }
}

bool AfcSlave::addDevice(const QString &id)
{
    if (m_devices.contains(id)) {
        return true;
    }

    auto *device = new AfcDevice(id);
    if (!device->isValid()) {
        delete device;
        return false;
    }

    m_devices.insert(id, device);

    if (device->name().isEmpty()) {
        return true; // Without a name isn't pretty but still valid
    }

    // FIXME append (2) or wahtever instead of not doing it
    if (!m_deviceUniqueNames.contains(device->name())) {
        m_deviceUniqueNames.insert(device->name(), id);
    }
    return true;
}

void AfcSlave::removeDevice(const QString &id)
{
    auto *device = m_devices.take(id);
    if (device) {
        const QString uniquePrettyName = m_deviceUniqueNames.key(id);
        if (!uniquePrettyName.isEmpty()) {
            m_deviceUniqueNames.remove(uniquePrettyName);
        }

        // Should we close it first?
        if (m_openDevice == device) {
            m_openDevice = nullptr;
        }

        delete device;
    }
}

void AfcSlave::listDir(const QUrl &url)
{
    if (redirectIfSolidUrl(url)) {
        return;
    }

    const auto afcUrl = this->afcUrl(url);

    if (afcUrl.deviceId().isEmpty()) {
        // Make sure refreshing the view will show devices that were trusted in the meantime
        bool foundInvalidDevices = false;
        updateDeviceList(&foundInvalidDevices);

        for (AfcDevice *device : m_devices) {
            const QString prettyName = m_deviceUniqueNames.key(device->id());
            UDSEntry entry = device->rootEntry(prettyName);

            // When there is only one device, redirect to it right away
            // We just read UDS_NAME so we get pretty name or ID without
            if (m_devices.count() == 1) {
                redirection(QUrl(QStringLiteral("afc:/%1").arg(entry.stringValue(UDSEntry::UDS_NAME))));
                finished();
                return;
            }

            listEntry(entry);
        }

        // We cannot just list that at the beginning because we might do a redirect
        listEntry(overviewEntry(QStringLiteral(".")));

        if (m_devices.isEmpty() && foundInvalidDevices) {
            error(ERR_SLAVE_DEFINED, i18n("An Apple device was found but it could not be accessed. Make sure that your device is unlocked and that it trusts this computer."));
            return;
        }

        finished();
        return;
    }

    AfcDevice *device = m_devices.value(afcUrl.deviceId());
    if (!device) {
        error(ERR_DOES_NOT_EXIST, afcUrl.deviceId());
        return;
    }

    // Ourself must be "."
    UDSEntry rootEntry;
    auto result = device->entry(afcUrl.path(), rootEntry);
    if (!result) {
        emitResult(result);
        return;
    }

    // NOTE this must not be "fastInsert" as AfcDevice::entry already sets a name
    rootEntry.replace(UDSEntry::UDS_NAME, QStringLiteral("."));
    listEntry(rootEntry);

    QStringList files;
    result = device->entryList(afcUrl.path(), files);
    if (!result) {
        emitResult(result);
        return;
    }

    for (const QString &file : files) {
        QString absolutePath = afcUrl.path();
        if (!absolutePath.endsWith(QLatin1Char('/'))
                && !file.startsWith(QLatin1Char('/'))) {
            absolutePath.append(QLatin1Char('/'));
        }
        absolutePath.append(file);

        UDSEntry entry;
        result = device->entry(absolutePath, entry);
        if (!result) {
            qCWarning(KIO_AFC_LOG) << "Failed to list" << absolutePath << result;
            continue;
        }

        listEntry(entry);
    }

    finished();
}

void AfcSlave::stat(const QUrl &url)
{
    if (redirectIfSolidUrl(url)) {
        return;
    }

    const auto afcUrl = this->afcUrl(url);

    if (afcUrl.deviceId().isEmpty()) {
        statEntry(overviewEntry());
        finished();
        return;
    }

    AfcDevice *device = m_devices.value(afcUrl.deviceId());
    if (!device) {
        this->error(ERR_DOES_NOT_EXIST, url.toDisplayString());
        return;
    }

    if (afcUrl.path().isEmpty()) {
        statEntry(device->rootEntry());
        finished();
        return;
    }

    UDSEntry entry;
    const auto result = device->entry(afcUrl.path(), entry);
    if (!result) {
        emitResult(result);
        return;
    }

    statEntry(entry);
    finished();
}

void AfcSlave::guessMimeType(AfcDevice *device, const QString &path)
{
    // Determine the mimetype of the file to be retrieved, and emit it.
    // This is mandatory in all slaves...

    AfcFileReader reader = device->fileReader();
    reader.setSize(1024);
    const auto result = reader.read();
    if (result) {
        QMimeDatabase db;
        const QString fileName = path.section(QLatin1Char('/'), -1, -1);
        QMimeType mime = db.mimeTypeForFileNameAndData(fileName, reader.data());
        mimeType(mime.name());
    }

    device->seek(0); // seek back to the start after our little peek
}

void AfcSlave::get(const QUrl &url)
{
    if (redirectIfSolidUrl(url)) {
        return;
    }

    const AfcUrl afcUrl = this->afcUrl(url);

    AfcDevice *device = m_devices.value(afcUrl.deviceId());
    if (!device) {
        this->error(ERR_DOES_NOT_EXIST, url.toDisplayString());
        return;
    }

    UDSEntry entry;
    auto result = device->entry(afcUrl.path(), entry);
    if (!result) {
        emitResult(result);
        return;
    }

    result = device->open(afcUrl.path(), QIODevice::ReadOnly);
    if (!result) {
        emitResult(result);
        return;
    }

    auto cleanup = qScopeGuard([device, url] {
        if (!device->close()) {
            qCWarning(KIO_AFC_LOG) << "Failed to close file after get" << url;
        }
    });

    const auto size = entry.numberValue(UDSEntry::UDS_SIZE, 0);
    totalSize(size);

    guessMimeType(device, afcUrl.path());

    position(0);

    AfcFileReader reader = device->fileReader();
    reader.setSize(size);

    while (reader.hasMore()) {
        const auto result = reader.read();
        if (!result) {
            emitResult(result);
            return;
        }
        data(reader.data());
    }

    finished();
}

void AfcSlave::put(const QUrl &url, int permissions, JobFlags flags)
{
    Q_UNUSED(permissions);
    const AfcUrl afcUrl = this->afcUrl(url);

    AfcDevice *device = m_devices.value(afcUrl.deviceId());
    if (!device) {
        this->error(ERR_DOES_NOT_EXIST, url.toDisplayString());
        return;
    }

    bool needToCloseFile = false;

    auto cleanup = qScopeGuard([&needToCloseFile, device] {
        if (needToCloseFile && !device->close()) {
            qCWarning(KIO_AFC_LOG) << "Failed to close file after put";
        }
    });

    UDSEntry entry;
    const auto getResult = device->entry(afcUrl.path(), entry);

    const bool exists = getResult.errorCode() != ERR_DOES_NOT_EXIST;
    if (exists && !flags.testFlag(KIO::Overwrite) && !flags.testFlag(KIO::Resume)) {
        if (S_ISDIR(entry.numberValue(UDSEntry::UDS_FILE_TYPE))) {
            error(ERR_DIR_ALREADY_EXIST, afcUrl.path());
            return;
        }
        error(ERR_FILE_ALREADY_EXIST, afcUrl.path());
        return;
    }

    if (!afcUrl.path().isEmpty()) {
        if (flags.testFlag(KIO::Resume)) {
            const auto result = device->open(afcUrl.path(), QIODevice::Append);
            if (!result) {
                emitResult(result);
                return;
            }
        } else {
            const auto result = device->open(afcUrl.path(), QIODevice::WriteOnly);
            if (!result) {
                emitResult(result);
                return;
            }
        }

        needToCloseFile = true;
    }

    int result = 0;

    do {
        QByteArray buffer;
        dataReq();
        // FIXME blocks when copying files within the same device
        result = readData(buffer);

        if (result < 0) {
            error(ERR_CANNOT_READ, QStringLiteral("readData result was %1").arg(result));
            return;
        }

        uint32_t bytesWritten = 0;
        const auto result = device->write(buffer, bytesWritten);
        if (!result) {
            emitResult(result);
            return;
        }
    } while (result > 0);

    if (!afcUrl.path().isEmpty()) {
        const QString modifiedMeta = metaData(QStringLiteral("modified"));

        if (!modifiedMeta.isEmpty()) {
            const QDateTime mtime = QDateTime::fromString(modifiedMeta, Qt::ISODate);

            if (mtime.isValid() && !device->setModificationTime(afcUrl.path(), mtime)) {
                qCWarning(KIO_AFC_LOG) << "Failed to set mtime for" << afcUrl.path() << "in put";
            }
        }
    }

    finished();
}

void AfcSlave::open(const QUrl &url, QIODevice::OpenMode mode)
{
    const AfcUrl afcUrl = this->afcUrl(url);

    AfcDevice *device = m_devices.value(afcUrl.deviceId());
    if (!device) {
        this->error(ERR_DOES_NOT_EXIST, url.toDisplayString());
        return;
    }

    UDSEntry entry;
    auto result = device->entry(afcUrl.path(), entry);
    if (!result) {
        emitResult(result);
        return;
    }

    result = device->open(afcUrl.path(), mode);
    if (!result) {
        emitResult(result);
        return;
    }

    if (mode.testFlag(QIODevice::ReadOnly) && !mode.testFlag(QIODevice::Append)) {
        guessMimeType(device, afcUrl.path());
    }

    m_openDevice = device;

    totalSize(entry.numberValue(UDSEntry::UDS_SIZE, 0));
    position(0);

    opened();
}

void AfcSlave::read(filesize_t bytesRequested)
{
    if (!m_openDevice) {
        error(KIO::ERR_CANNOT_SEEK, i18n("Cannot read without opening file first"));
        return;
    }

    AfcFileReader reader = m_openDevice->fileReader();
    reader.setSize(bytesRequested);

    while (reader.hasMore()) {
        const auto result = reader.read();
        if (!result) {
            emitResult(result);
            return;
        }
        data(reader.data());
    }
}

void AfcSlave::seek(filesize_t offset)
{
    if (!m_openDevice) {
        error(KIO::ERR_CANNOT_SEEK, QStringLiteral("Cannot seek without opening file first"));
        return;
    }

    const auto result = m_openDevice->seek(offset);
    if (!result) {
        emitResult(result);
        return;
    }

    position(offset);
}

void AfcSlave::write(const QByteArray &data)
{
    if (!m_openDevice) {
        error(KIO::ERR_CANNOT_SEEK, QStringLiteral("Cannot write without opening file first"));
        return;
    }

    uint32_t bytesWritten = 0;
    const auto result = m_openDevice->write(data, bytesWritten);

    if (!result) {
        emitResult(result);
        return;
    }

    written(bytesWritten);
}

void AfcSlave::close()
{
    if (!m_openDevice) {
        error(KIO::ERR_INTERNAL, i18n("Cannot close what is not open"));
        return;
    }

    const auto result = m_openDevice->close();
    if (!result) {
        emitResult(result);
        return;
    }

    m_openDevice = nullptr;
}

void AfcSlave::del(const QUrl &url, bool isFile)
{
    Q_UNUSED(isFile)
    const AfcUrl afcUrl = this->afcUrl(url);

    AfcDevice *device = m_devices.value(afcUrl.deviceId());
    if (!device) {
        this->error(ERR_DOES_NOT_EXIST, url.toDisplayString());
        return;
    }

    emitResult(device->del(afcUrl.path()));
}

void AfcSlave::rename(const QUrl &url, const QUrl &dest, JobFlags flags)
{
    const AfcUrl srcAfcUrl = afcUrl(url);
    const AfcUrl destAfcUrl = afcUrl(dest);

    if (srcAfcUrl.deviceId() != destAfcUrl.deviceId()) {
        error(ERR_CANNOT_RENAME, i18n("Cannot rename between devices."));
        return;
    }

    AfcDevice *device = m_devices.value(srcAfcUrl.deviceId());
    if (!device) {
        this->error(ERR_DOES_NOT_EXIST, url.toDisplayString());
        return;
    }

    emitResult(device->rename(srcAfcUrl.path(), destAfcUrl.path(), flags));
}

void AfcSlave::symlink(const QString &target, const QUrl &dest, JobFlags flags)
{
    QUrl targetUrl;
    targetUrl.setScheme(QStringLiteral("afc"));
    targetUrl.setPath(target);

    // Turning it into a QUrl so we can resolve both device ID and unique name
    const AfcUrl targetAfcUrl = afcUrl(targetUrl);
    const AfcUrl destAfcUrl = afcUrl(dest);

    if (targetAfcUrl.deviceId() != destAfcUrl.deviceId()) {
        error(ERR_CANNOT_SYMLINK, i18n("Cannot symlink between devices."));
        return;
    }

    AfcDevice *device = m_devices.value(destAfcUrl.deviceId());
    if (!device) {
        this->error(ERR_DOES_NOT_EXIST, dest.toDisplayString());
        return;
    }

    emitResult(device->symlink(targetAfcUrl.path(), destAfcUrl.path(), flags));
}

void AfcSlave::mkdir(const QUrl &url, int permissions)
{
    Q_UNUSED(permissions)

    const AfcUrl afcUrl = this->afcUrl(url);

    AfcDevice *device = m_devices.value(afcUrl.deviceId());
    if (!device) {
        this->error(ERR_DOES_NOT_EXIST, url.toDisplayString());
        return;
    }

    emitResult(device->mkdir(afcUrl.path()));
}

void AfcSlave::setModificationTime(const QUrl &url, const QDateTime &mtime)
{
    const AfcUrl afcUrl = this->afcUrl(url);

    AfcDevice *device = m_devices.value(afcUrl.deviceId());
    if (!device) {
        this->error(ERR_DOES_NOT_EXIST, url.toDisplayString());
        return;
    }

    emitResult(device->setModificationTime(afcUrl.path(), mtime));
}

void AfcSlave::fileSystemFreeSpace(const QUrl &url)
{
    const QUrl redirectUrl = resolveSolidUrl(url);
    // TODO FileSystemFreeSpaceJob does not follow redirects
    if (redirectUrl.isValid()) {
        fileSystemFreeSpace(redirectUrl);
        return;
    }

    const AfcUrl afcUrl = this->afcUrl(url);

    AfcDevice *device = m_devices.value(afcUrl.deviceId());
    if (!device) {
        this->error(ERR_DOES_NOT_EXIST, url.toDisplayString());
        return;
    }

    const AfcDiskUsage diskUsage = device->diskUsage();
    if (!diskUsage.isValid()) {
        this->error(ERR_CANNOT_STAT, url.toDisplayString());
        return;
    }

    setMetaData(QStringLiteral("total"), QString::number(diskUsage.totalDiskCapacity()));
    setMetaData(QStringLiteral("available"), QString::number(diskUsage.totalDataAvailable()));
    finished();
}

void AfcSlave::cotruncate(filesize_t length)
{
    if (!m_openDevice) {
        error(KIO::ERR_CANNOT_TRUNCATE, QStringLiteral("Cannot truncate without opening file first"));
        return;
    }

    const auto result = m_openDevice->truncate(length);
    if (!result) {
        emitResult(result);
        return;
    }

    truncated(length);
}

void AfcSlave::virtual_hook(int id, void *data)
{
    switch(id) {
    case SlaveBase::GetFileSystemFreeSpace: {
        QUrl *url = static_cast<QUrl *>(data);
        fileSystemFreeSpace(*url);
    }
    break;
    case SlaveBase::Truncate: {
        const auto length = static_cast<KIO::filesize_t *>(data);
        cotruncate(*length);
    }
    break;
    default:
        SlaveBase::virtual_hook(id, data);
    }
}

extern "C"
{
    int Q_DECL_EXPORT kdemain(int argc, char **argv)
    {
        QCoreApplication app(argc, argv);
        app.setApplicationName(QStringLiteral("kio_afc"));

        AfcSlave slave(argv[2], argv[3]);
        slave.dispatchLoop();
        return 0;
    }
}

#include "kio_afc.moc"
