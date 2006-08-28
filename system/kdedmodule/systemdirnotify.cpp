/* This file is part of the KDE Project
   Copyright (c) 2004 Kévin Ottens <ervin ipsquad net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "systemdirnotify.h"

#include <kdebug.h>
#include <klocale.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <kdesktopfile.h>
#include <kdirnotify.h>

#include <QDir>

SystemDirNotify::SystemDirNotify()
: mInited( false )
{
	QDBusConnection::sessionBus().connect(QString(), QString(), "org.kde.KDirNotify",
				    "FilesAdded", this, SLOT(FilesAdded(QString))); QDBusConnection::sessionBus().connect(QString(), QString(), "org.kde.KDirNotify",
				    "FilesRemoved", this, SLOT(FilesRemoved(QStringList))); QDBusConnection::sessionBus().connect(QString(), QString(), "org.kde.KDirNotify",
				    "FilesChanged", this, SLOT(FilesChanged(QStringList)));
}

void SystemDirNotify::init()
{
	if( mInited )
		return;
	mInited = true;
	KGlobal::dirs()->addResourceType("system_entries",
		KStandardDirs::kde_default("data") + "systemview");

	QStringList names_found;
	QStringList dirList = KGlobal::dirs()->resourceDirs("system_entries");

	QStringList::ConstIterator dirpath = dirList.begin();
	QStringList::ConstIterator end = dirList.end();
	for(; dirpath!=end; ++dirpath)
	{
		QDir dir = *dirpath;
		if (!dir.exists()) continue;

		QStringList filenames
			= dir.entryList( QDir::Files | QDir::Readable );

		QStringList::ConstIterator name = filenames.begin();
		QStringList::ConstIterator endf = filenames.end();

		for(; name!=endf; ++name)
		{
			if (!names_found.contains(*name))
			{
				KDesktopFile desktop(*dirpath+*name, true);

				QString system_name = *name;
				system_name.truncate(system_name.length()-8);

				KUrl system_url("system:/"+system_name);

				if ( !desktop.readUrl().isEmpty() )
				{
					m_urlMap[desktop.readUrl()] = system_url;
					names_found.append( *name );
				}
				else if ( !desktop.readPath().isEmpty() )
				{
					KUrl url;
					url.setPath( desktop.readPath() );
					m_urlMap[url] = system_url;
					names_found.append( *name );
				}
			}
		}
	}
}

KUrl SystemDirNotify::toSystemURL(const KUrl &url)
{
	kDebug() << "SystemDirNotify::toSystemURL(" << url << ")" << endl;

	init();
	QMap<KUrl,KUrl>::const_iterator it = m_urlMap.begin();
	QMap<KUrl,KUrl>::const_iterator end = m_urlMap.end();

	for (; it!=end; ++it)
	{
		KUrl base = it.key();

		if ( base.isParentOf(url) )
		{
			QString path = KUrl::relativePath(base.path(),
			                                  url.path());
			KUrl result = it.value();
			result.addPath(path);
			result.cleanPath();
			kDebug() << result << endl;
			return result;
		}
	}

	kDebug() << "KUrl()" << endl;
	return KUrl();
}

KUrl::List SystemDirNotify::toSystemURLList(const KUrl::List &list)
{
	init();
	KUrl::List new_list;

	KUrl::List::const_iterator it = list.begin();
	KUrl::List::const_iterator end = list.end();

	for (; it!=end; ++it)
	{
		KUrl url = toSystemURL(*it);

		if (url.isValid())
		{
			new_list.append(url);
		}
	}

	return new_list;
}

void SystemDirNotify::FilesAdded(const QString &directory)
{
	KUrl new_dir = toSystemURL(directory);

	if (new_dir.isValid())
	{
		org::kde::KDirNotify::emitFilesAdded( new_dir.url() );
		if (new_dir.upUrl().upUrl()==KUrl("system:/"))
		{
			org::kde::KDirNotify::emitFilesChanged( QStringList() << new_dir.upUrl().url() );
		}
	}
}

void SystemDirNotify::FilesRemoved(const QStringList &fileList)
{
	KUrl::List new_list = toSystemURLList(fileList);

	if (!new_list.isEmpty())
	{
		org::kde::KDirNotify::emitFilesRemoved( new_list.toStringList() );

		KUrl::List::const_iterator it = new_list.begin();
		KUrl::List::const_iterator end = new_list.end();

		for (; it!=end; ++it)
		{
			if ((*it).upUrl().upUrl()==KUrl("system:/"))
			{
				org::kde::KDirNotify::emitFilesChanged( QStringList() << (*it).upUrl().url() );
			}
		}
	}
}

void SystemDirNotify::FilesChanged(const QStringList &fileList)
{
	KUrl::List new_list = toSystemURLList(fileList);

	if (!new_list.isEmpty())
	{
		org::kde::KDirNotify::emitFilesChanged( new_list.toStringList() );
	}
}

#include "systemdirnotify.moc"
