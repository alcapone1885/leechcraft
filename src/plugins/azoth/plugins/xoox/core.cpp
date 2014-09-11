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

#include "core.h"
#include <QFile>
#include <QXmlStreamWriter>
#include <QDomDocument>
#include <QTimer>
#include <QDir>
#include <QXmppLogger.h>
#include <util/sys/paths.h>
#include <interfaces/azoth/iaccount.h>
#include <interfaces/azoth/iproxyobject.h>
#include "glooxprotocol.h"
#include "glooxclentry.h"
#include "glooxaccount.h"
#include "capsdatabase.h"
#include "avatarsstorage.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Xoox
{
	Core::Core ()
	: PluginProxy_ (0)
	, SaveRosterScheduled_ (false)
	, CapsDB_ (new CapsDatabase (this))
	, Avatars_ (new AvatarsStorage (this))
	{
		QXmppLogger::getLogger ()->setLoggingType (QXmppLogger::FileLogging);
		QXmppLogger::getLogger ()->setLogFilePath (Util::CreateIfNotExists ("azoth").filePath ("qxmpp.log"));
		QXmppLogger::getLogger ()->setMessageTypes (QXmppLogger::AnyMessage);
		GlooxProtocol_.reset (new GlooxProtocol (this));
	}

	Core& Core::Instance ()
	{
		static Core c;
		return c;
	}

	void Core::SecondInit ()
	{
		GlooxProtocol_->SetProxyObject (PluginProxy_);
		GlooxProtocol_->Prepare ();
		LoadRoster ();
		for (const auto account : GlooxProtocol_->GetRegisteredAccounts ())
			connect (account,
					SIGNAL (gotCLItems (QList<QObject*>)),
					this,
					SLOT (handleItemsAdded (QList<QObject*>)));
	}

	void Core::Release ()
	{
		GlooxProtocol_.reset ();
	}

	QList<QObject*> Core::GetProtocols () const
	{
		return { GlooxProtocol_.get () };
	}

	void Core::SetPluginProxy (QObject *proxy)
	{
		PluginProxy_ = proxy;
	}

	IProxyObject* Core::GetPluginProxy () const
	{
		return qobject_cast<IProxyObject*> (PluginProxy_);
	}

	void Core::SetProxy (ICoreProxy_ptr proxy)
	{
		Proxy_ = proxy;
	}

	ICoreProxy_ptr Core::GetProxy () const
	{
		return Proxy_;
	}

	CapsDatabase* Core::GetCapsDatabase () const
	{
		return CapsDB_;
	}

	AvatarsStorage* Core::GetAvatarsStorage () const
	{
		return Avatars_;
	}

	void Core::SendEntity (const Entity& e)
	{
		emit gotEntity (e);
	}

	void Core::ScheduleSaveRoster (int hint)
	{
		if (SaveRosterScheduled_)
			return;

		SaveRosterScheduled_ = true;
		QTimer::singleShot (hint,
				this,
				SLOT (saveRoster ()));
	}

	namespace
	{
		struct EntryData
		{
			QByteArray ID_;
			QString Name_;
		};
	}

	void Core::LoadRoster ()
	{
		QFile rosterFile (Util::CreateIfNotExists ("azoth/xoox")
					.absoluteFilePath ("roster.xml"));
		if (!rosterFile.exists ())
			return;
		if (!rosterFile.open (QIODevice::ReadOnly))
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to open roster file"
					<< rosterFile.fileName ()
					<< rosterFile.errorString ();
			return;
		}

		QDomDocument doc;
		QString errStr;
		int errLine = 0;
		int errCol = 0;
		if (!doc.setContent (&rosterFile, &errStr, &errLine, &errCol))
		{
			qWarning () << Q_FUNC_INFO
					<< errStr
					<< errLine
					<< ":"
					<< errCol;
			return;
		}

		QDomElement root = doc.firstChildElement ("roster");
		if (root.attribute ("formatversion") != "1")
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown format version"
					<< root.attribute ("formatversion");
			return;
		}

		QMap<QByteArray, GlooxAccount*> id2account;
		Q_FOREACH (QObject *accObj,
				GlooxProtocol_->GetRegisteredAccounts ())
		{
			GlooxAccount *acc = qobject_cast<GlooxAccount*> (accObj);
			id2account [acc->GetAccountID ()] = acc;
		}

		QDomElement account = root.firstChildElement ("account");
		while (!account.isNull ())
		{
			const QByteArray& id = account.firstChildElement ("id").text ().toUtf8 ();
			if (id.isEmpty ())
			{
				qWarning () << Q_FUNC_INFO
						<< "empty ID";
				continue;
			}

			if (!id2account.contains (id))
			{
				account = account.nextSiblingElement ("account");
				continue;
			}

			QDomElement entry = account
					.firstChildElement ("entries")
					.firstChildElement ("entry");
			while (!entry.isNull ())
			{
				const QByteArray& entryID =
						QByteArray::fromPercentEncoding (entry
								.firstChildElement ("id").text ().toLatin1 ());

				if (entryID.isEmpty ())
					qWarning () << Q_FUNC_INFO
							<< "entry ID is empty";
				else
				{
					OfflineDataSource_ptr ods (new OfflineDataSource);
					Load (ods, entry);

					GlooxCLEntry *clEntry = id2account [id]->CreateFromODS (ods);

					const QString& path = Util::CreateIfNotExists ("azoth/xoox/avatars")
							.absoluteFilePath (entryID.toBase64 ());
					clEntry->SetAvatar (QImage (path, "PNG"));
				}
				entry = entry.nextSiblingElement ("entry");
			}

			account = account.nextSiblingElement ("account");
		}
	}

	void Core::saveRoster ()
	{
		SaveRosterScheduled_ = false;
		QFile rosterFile (Util::CreateIfNotExists ("azoth/xoox")
					.absoluteFilePath ("roster.xml"));
		if (!rosterFile.open (QIODevice::WriteOnly | QIODevice::Truncate))
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to open file"
					<< rosterFile.fileName ()
					<< rosterFile.errorString ();
			return;
		}

		QXmlStreamWriter w (&rosterFile);
		w.setAutoFormatting (true);
		w.setAutoFormattingIndent (2);
		w.writeStartDocument ();
		w.writeStartElement ("roster");
		w.writeAttribute ("formatversion", "1");
		for (auto accObj : GlooxProtocol_->GetRegisteredAccounts ())
		{
			auto acc = qobject_cast<IAccount*> (accObj);
			w.writeStartElement ("account");
				w.writeTextElement ("id", acc->GetAccountID ());
				w.writeStartElement ("entries");
				for (auto entryObj : acc->GetCLEntries ())
				{
					const auto entry = qobject_cast<GlooxCLEntry*> (entryObj);
					if (!entry ||
							(entry->GetEntryFeatures () & ICLEntry::FMaskLongetivity) != ICLEntry::FPermanentEntry)
						continue;

					Save (entry->ToOfflineDataSource (), &w);
					saveAvatarFor (entry);
				}
				w.writeEndElement ();
			w.writeEndElement ();
		}
		w.writeEndElement ();
		w.writeEndDocument ();
	}

	void Core::handleItemsAdded (const QList<QObject*>& items)
	{
		bool shouldSave = false;
		for (auto clEntry : items)
		{
			auto entry = qobject_cast<GlooxCLEntry*> (clEntry);
			if (!entry ||
					(entry->GetEntryFeatures () & ICLEntry::FMaskLongetivity) != ICLEntry::FPermanentEntry)
				continue;

			shouldSave = true;

			connect (entry,
					SIGNAL (avatarChanged (const QImage&)),
					this,
					SLOT (saveAvatarFor ()),
					Qt::UniqueConnection);
		}

		if (shouldSave)
			ScheduleSaveRoster (5000);
	}

	void Core::saveAvatarFor (GlooxCLEntry *entry)
	{
		const bool lazy = entry;
		if (!entry)
			entry = qobject_cast<GlooxCLEntry*> (sender ());

		if (!entry)
		{
			qWarning () << Q_FUNC_INFO
					<< "null parameter and wrong sender():"
					<< sender ();
			return;
		}

		const QByteArray& filename = entry->GetEntryID ().toUtf8 ().toBase64 ();
		const QDir& avatarDir = Util::CreateIfNotExists ("azoth/xoox/avatars");
		if (lazy && avatarDir.exists (filename))
			return;

		const QString& path = avatarDir.absoluteFilePath (filename);
		entry->GetAvatar ().save (path, "PNG", 100);
	}
}
}
}
