
/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2011  Georg Rudoy
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#include "woodpecker.h"
#include <QTranslator>
#include <QIcon>
#include <interfaces/entitytesthandleresult.h>
#include <xmlsettingsdialog/xmlsettingsdialog.h>
#include <util/util.h>
#include "core.h"
#include "twitterpage.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace Woodpecker
{
void Plugin::Init (ICoreProxy_ptr proxy)
{
	TwitterPage::SetParentMultiTabs (this);

	Translator_.reset (Util::InstallTranslator ("woodpecker"));

	XmlSettingsDialog_.reset (new Util::XmlSettingsDialog ());
	XmlSettingsDialog_->RegisterObject (XmlSettingsManager::Instance (),
										"woodpeckersettings.xml");

	Core::Instance ().SetProxy (proxy);

	connect (&Core::Instance (),
			 SIGNAL (addNewTab (const QString&, QWidget*)),
			 this,
			 SIGNAL (addNewTab (const QString&, QWidget*)));
	connect (&Core::Instance (),
			 SIGNAL (removeTab (QWidget*)),
			 this,
			 SIGNAL (removeTab (QWidget*)));
	connect (&Core::Instance (),
			 SIGNAL (raiseTab (QWidget*)),
			 this,
			 SIGNAL (raiseTab (QWidget*)));
	connect (&Core::Instance (),
			 SIGNAL (changeTabName (QWidget*, const QString&)),
			 this,
			 SIGNAL (changeTabName (QWidget*, const QString&)));
	connect (&Core::Instance (),
			 SIGNAL (changeTabIcon (QWidget*, const QIcon&)),
			 this,
			 SIGNAL (changeTabIcon (QWidget*, const QIcon&)));
	connect (&Core::Instance (),
			 SIGNAL (couldHandle (const LeechCraft::Entity&, bool*)),
			 this,
			 SIGNAL (couldHandle (const LeechCraft::Entity&, bool*)));
	connect (&Core::Instance (),
			 SIGNAL (delegateEntity (const LeechCraft::Entity&,
									 int*, QObject**)),
			 this,
			 SIGNAL (delegateEntity (const LeechCraft::Entity&,
									 int*, QObject**)));
	connect (&Core::Instance (),
			 SIGNAL (gotEntity (const LeechCraft::Entity&)),
			 this,
			 SIGNAL (gotEntity (const LeechCraft::Entity&)));
}

void Plugin::SecondInit ()
{
}

void Plugin::Release ()
{
}

QByteArray Plugin::GetUniqueID () const
{
	return "org.LeechCraft.Woodpecker";
}

QString Plugin::GetName () const
{
	return "Woodpecker";
}

QString Plugin::GetInfo () const
{
	return tr ("Simple twitter client");
}

QIcon Plugin::GetIcon () const
{
	return QIcon (":/resources/images/woodpecker.svg");
}

TabClasses_t Plugin::GetTabClasses () const
{
	TabClasses_t result;
	result << Core::Instance ().GetTabClass ();
	return result;
}

void Plugin::TabOpenRequested (const QByteArray& tabClass)
{
	if (tabClass == "Woodpecker")
		Core::Instance ().NewTabRequested ();
	else
		qWarning () << Q_FUNC_INFO
					<< "unknown tab class"
					<< tabClass;
}


std::shared_ptr<Util::XmlSettingsDialog> Plugin::GetSettingsDialog () const
{
	return XmlSettingsDialog_;
}
}
}

LC_EXPORT_PLUGIN (leechcraft_woodpecker, LeechCraft::Woodpecker::Plugin);

// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
