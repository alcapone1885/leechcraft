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

#pragma once

#include <memory>
#include <QObject>
#include <QDir>
#include <QHash>
#include <QSet>

extern "C"
{
#include <libotr/proto.h>
#include <libotr/message.h>
}

#include <interfaces/azoth/imessage.h>
#include <interfaces/structures.h>
#include <interfaces/core/ihookproxy.h>
#include <interfaces/core/icoreproxy.h>

class QTimer;
class QMenu;
class QAction;

namespace LeechCraft
{
namespace Azoth
{
class IProxyObject;

namespace OTRoid
{
	class Authenticator;
	enum class SmpMethod;

	class OtrHandler : public QObject
	{
		Q_OBJECT

		const ICoreProxy_ptr CoreProxy_;
		IProxyObject * const AzothProxy_;

		const QDir OtrDir_;

		const OtrlUserState UserState_;
		OtrlMessageAppOps OtrOps_;

#if OTRL_VERSION_MAJOR >= 4
		QTimer *PollTimer_;
#endif
		struct EntryActions
		{
			std::shared_ptr<QMenu> CtxMenu_;
			std::shared_ptr<QMenu> ButtonMenu_;

			std::shared_ptr<QAction> ToggleOtr_;
			std::shared_ptr<QAction> ToggleOtrCtx_;
			std::shared_ptr<QAction> Authenticate_;
		};
		QHash<QObject*, EntryActions> Entry2Action_;

		QHash<QObject*, QString> Msg2OrigText_;

		QSet<QObject*> PendingInjectedMessages_;

		bool IsGenerating_ = false;

		QHash<ICLEntry*, Authenticator*> Auths_;
	public:
		OtrHandler (const ICoreProxy_ptr&, IProxyObject*);
		~OtrHandler ();

		void HandleMessageCreated (const IHookProxy_ptr&, IMessage*);
		void HandleGotMessage (const IHookProxy_ptr&, QObject*);
		void HandleEntryActionsRemoved (QObject*);
		void HandleEntryActionsRequested (const IHookProxy_ptr&, QObject*);
		void HandleEntryActionAreasRequested (const IHookProxy_ptr&, QObject*);

		OtrlUserState GetUserState () const;

		int IsLoggedIn (const QString& accId, const QString& entryId);
		void InjectMsg (const QString& accId, const QString& entryId,
				const QString& msg, bool hidden, IMessage::Direction,
				IMessage::Type = IMessage::Type::ChatMessage);
		void InjectMsg (ICLEntry *entry,
				const QString& msg, bool hidden, IMessage::Direction,
				IMessage::Type = IMessage::Type::ChatMessage);
		void Notify (const QString& accId, const QString& entryId,
				Priority, const QString& title,
				const QString& primary, const QString& secondary);
		QString GetAccountName (const QString& accId);
		QString GetVisibleEntryName (const QString& accId, const QString& entryId);

		void CreatePrivkey (const char*, const char*, bool confirm = true);
#if OTRL_VERSION_MAJOR >= 4
		void CreateInstag (const char*, const char*);

		void SetPollTimerInterval (unsigned int seconds);
		void HandleSmpEvent (OtrlSMPEvent, ConnContext*, unsigned short, const QString&);
#endif
	private:
		QByteArray GetOTRFilename (const QString&) const;

		void CreateActions (QObject*);
		void SetOtrState (ICLEntry*, bool);

#if OTRL_VERSION_MAJOR >= 4
		void CreateAuthForEntry (ICLEntry*);
#endif
	public slots:
		void writeFingerprints ();
		void writeKeys ();

		void generateKeys (const QString&, const QString&);
	private slots:
		void handleOtrAction ();
#if OTRL_VERSION_MAJOR >= 4
		void handleAuthRequested ();
		void startAuth (ICLEntry*, SmpMethod, const QString&, const QString&);
		void handleAuthDestroyed ();

		void handleGotSmpReply (SmpMethod, const QString&, ConnContext*);
		void handleAbortSmp (ConnContext*);

		void pollOTR ();
#endif
	signals:
		void privKeysChanged ();
	};
}
}
}
