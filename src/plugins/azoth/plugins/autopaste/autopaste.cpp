/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2014  Georg Rudoy
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "autopaste.h"
#include <QIcon>
#include <QMessageBox>
#include <xmlsettingsdialog/xmlsettingsdialog.h>
#include <util/util.h>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/azoth/iclentry.h>
#include "xmlsettingsmanager.h"
#include "codepadservice.h"
#include "pastedialog.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Autopaste
{
	void Plugin::Init (ICoreProxy_ptr proxy)
	{
		Util::InstallTranslator ("azoth_autopaste");

		Proxy_ = proxy;

		XmlSettingsDialog_.reset (new Util::XmlSettingsDialog);
		XmlSettingsDialog_->RegisterObject (&XmlSettingsManager::Instance (),
				"azothautopastesettings.xml");
	}

	void Plugin::SecondInit ()
	{
	}

	QByteArray Plugin::GetUniqueID () const
	{
		return "org.LeechCraft.Azoth.Autopaste";
	}

	void Plugin::Release ()
	{
	}

	QString Plugin::GetName () const
	{
		return "Azoth Autopaste";
	}

	QString Plugin::GetInfo () const
	{
		return tr ("Detects long messages and suggests pasting them to a pastebin.");
	}

	QIcon Plugin::GetIcon () const
	{
		static QIcon icon ("lcicons:/plugins/azoth/plugins/autopaste/resources/images/autopaste.svg");
		return icon;
	}

	QSet<QByteArray> Plugin::GetPluginClasses () const
	{
		QSet<QByteArray> result;
		result << "org.LeechCraft.Plugins.Azoth.Plugins.IGeneralPlugin";
		return result;
	}

	Util::XmlSettingsDialog_ptr Plugin::GetSettingsDialog () const
	{
		return XmlSettingsDialog_;
	}

	void Plugin::hookMessageWillCreated (LeechCraft::IHookProxy_ptr proxy,
			QObject*, QObject *entry, int, QString)
	{
		ICLEntry *other = qobject_cast<ICLEntry*> (entry);
		if (!other)
		{
			qWarning () << Q_FUNC_INFO
				<< "unable to cast"
				<< entry
				<< "to ICLEntry";
			return;
		}

		const auto& text = proxy->GetValue ("text").toString ();

		const int maxLines = XmlSettingsManager::Instance ()
				.property ("LineCount").toInt ();
		const int maxSymbols = XmlSettingsManager::Instance ()
				.property ("SymbolCount").toInt ();
		if (text.size () < maxSymbols &&
				text.count ('\n') + 1 < maxLines)
			return;

		QByteArray propName;
		switch (other->GetEntryType ())
		{
		case ICLEntry::EntryType::Chat:
			propName = "EnableForNormalChats";
			break;
		case ICLEntry::EntryType::MUC:
			propName = "EnableForMUCChats";
			break;
		case ICLEntry::EntryType::PrivateChat:
			propName = "EnableForPrivateChats";
			break;
		default:
			return;
		}

		if (!XmlSettingsManager::Instance ().property (propName).toBool ())
			return;

		QSettings settings (QCoreApplication::organizationName (),
				QCoreApplication::applicationName () + "_Azoth_Autopaste");
		settings.beginGroup ("SavedChoices");
		settings.beginGroup (other->GetEntryID ());
		auto guard = std::shared_ptr<void> (nullptr,
				[&settings] (void*) -> void
				{
					settings.endGroup ();
					settings.endGroup ();
				});

		PasteDialog dia;

		dia.SetCreatorName (settings.value ("Service").toString ());
		dia.SetHighlight (static_cast<Highlight> (settings.value ("Highlight").toInt ()));

		dia.exec ();

		switch (dia.GetChoice ())
		{
		case PasteDialog::Cancel:
			proxy->CancelDefault ();
			proxy->SetValue ("PreserveMessageEdit", true);
			break;
		case PasteDialog::No:
			break;
		case PasteDialog::Yes:
		{
			auto service = dia.GetCreator () (entry);
			service->Paste ({ Proxy_->GetNetworkAccessManager (), text, dia.GetHighlight () });
			proxy->CancelDefault ();

			settings.setValue ("Service", dia.GetCreatorName ());
			settings.setValue ("Highlight", static_cast<int> (dia.GetHighlight ()));
			break;
		}
		}
	}
}
}
}

LC_EXPORT_PLUGIN (leechcraft_azoth_autopaste, LeechCraft::Azoth::Autopaste::Plugin);
