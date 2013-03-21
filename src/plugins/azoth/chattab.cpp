/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
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

#include "chattab.h"
#include <QWebFrame>
#include <QWebElement>
#include <QTextDocument>
#include <QTimer>
#include <QBuffer>
#include <QPalette>
#include <QApplication>
#include <QShortcut>
#include <QMenu>
#include <QFileDialog>
#include <QMessageBox>
#include <QKeyEvent>
#include <QTextBrowser>
#include <QDesktopWidget>
#include <util/defaulthookproxy.h>
#include <util/util.h>
#include <util/shortcuts/shortcutmanager.h>
#include <util/gui/util.h>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/ipluginsmanager.h>
#include <interfaces/core/ientitymanager.h>
#include "interfaces/azoth/iclentry.h"
#include "interfaces/azoth/imessage.h"
#include "interfaces/azoth/irichtextmessage.h"
#include "interfaces/azoth/iaccount.h"
#include "interfaces/azoth/imucentry.h"
#include "interfaces/azoth/itransfermanager.h"
#include "interfaces/azoth/iconfigurablemuc.h"
#include "interfaces/azoth/ichatstyleresourcesource.h"
#include "interfaces/azoth/isupportmediacalls.h"
#include "interfaces/azoth/imediacall.h"
#include "interfaces/azoth/ihistoryplugin.h"
#include "interfaces/azoth/imucperms.h"
#ifdef ENABLE_CRYPT
#include "interfaces/azoth/isupportpgp.h"
#endif
#include "core.h"
#include "textedit.h"
#include "chattabsmanager.h"
#include "xmlsettingsmanager.h"
#include "transferjobmanager.h"
#include "bookmarksmanagerdialog.h"
#include "simpledialog.h"
#include "zoomeventfilter.h"
#include "callmanager.h"
#include "callchatwidget.h"
#include "chattabwebview.h"
#include "msgformatterwidget.h"
#include "actionsmanager.h"
#include "contactdropfilter.h"
#include "userslistwidget.h"
#include "util.h"
#include "proxyobject.h"

namespace LeechCraft
{
namespace Azoth
{
	QObject *ChatTab::S_ParentMultiTabs_ = 0;
	TabClassInfo ChatTab::S_ChatTabClass_;
	TabClassInfo ChatTab::S_MUCTabClass_;

	void ChatTab::SetParentMultiTabs (QObject *obj)
	{
		S_ParentMultiTabs_ = obj;
	}

	void ChatTab::SetChatTabClassInfo (const TabClassInfo& tc)
	{
		S_ChatTabClass_ = tc;
	}

	void ChatTab::SetMUCTabClassInfo (const TabClassInfo& tc)
	{
		S_MUCTabClass_ = tc;
	}

	class CopyFilter : public QObject
	{
		QWebView *View_;
	public:
		CopyFilter (QWebView *v)
		: QObject (v)
		, View_ (v)
		{
		}
	protected:
		bool eventFilter (QObject*, QEvent *orig)
		{
			if (orig->type () != QEvent::KeyRelease)
				return false;

			QKeyEvent *e = static_cast<QKeyEvent*> (orig);
			if (e->matches (QKeySequence::Copy) &&
					!View_->page ()->selectedText ().isEmpty ())
			{
				View_->pageAction (QWebPage::Copy)->trigger ();
				return true;
			}

			return false;
		}
	};

	ChatTab::ChatTab (const QString& entryId,
			QWidget *parent)
	: QWidget (parent)
	, TabToolbar_ (new QToolBar (tr ("Azoth chat window"), this))
	, MUCEventLog_ (new QTextBrowser ())
	, ToggleRichText_ (0)
	, Call_ (0)
#ifdef ENABLE_CRYPT
	, EnableEncryption_ (0)
#endif
	, EntryID_ (entryId)
	, BgColor_ (QApplication::palette ().color (QPalette::Base))
	, CurrentHistoryPosition_ (-1)
	, CurrentNickIndex_ (0)
	, LastSpacePosition_(-1)
	, HadHighlight_ (false)
	, NumUnreadMsgs_ (0)
	, ScrollbackPos_ (0)
	, IsMUC_ (false)
	, PreviousTextHeight_ (0)
	, MsgFormatter_ (0)
	, TypeTimer_ (new QTimer (this))
	, PreviousState_ (CPSNone)
	{
		Ui_.setupUi (this);
		Ui_.View_->installEventFilter (new ZoomEventFilter (Ui_.View_));
		Ui_.MsgEdit_->installEventFilter (new CopyFilter (Ui_.View_));
		MUCEventLog_->installEventFilter (this);

		auto dropFilter = new ContactDropFilter (this);
		Ui_.View_->installEventFilter (dropFilter);
		Ui_.MsgEdit_->installEventFilter (dropFilter);
		connect (dropFilter,
				SIGNAL (localImageDropped (QImage, QUrl)),
				this,
				SLOT (handleLocalImageDropped (QImage, QUrl)));
		connect (dropFilter,
				SIGNAL (imageDropped (QImage)),
				this,
				SLOT (handleImageDropped (QImage)));
		connect (dropFilter,
				SIGNAL (filesDropped (QList<QUrl>)),
				this,
				SLOT (handleFilesDropped (QList<QUrl>)));

		Ui_.SubjBox_->setVisible (false);
		Ui_.SubjChange_->setEnabled (false);

		Ui_.EventsButton_->setMenu (new QMenu (tr ("Events"), this));
		Ui_.EventsButton_->hide ();

		Ui_.SendButton_->setIcon (Core::Instance ().GetProxy ()->GetIcon ("key-enter"));
		connect (Ui_.SendButton_,
				SIGNAL (released ()),
				this,
				SLOT (messageSend ()));

		BuildBasicActions ();

		Core::Instance ().RegisterHookable (this);

		connect (Core::Instance ().GetTransferJobManager (),
				SIGNAL (jobNoLongerOffered (QObject*)),
				this,
				SLOT (handleFileNoLongerOffered (QObject*)));

		QSize ccSize = Ui_.CharCounter_->size ();
		ccSize.setWidth (QApplication::fontMetrics ().width (" 9999"));
		Ui_.CharCounter_->resize (ccSize);

		Ui_.View_->page ()->setLinkDelegationPolicy (QWebPage::DelegateAllLinks);

		connect (Ui_.View_,
				SIGNAL (linkClicked (QUrl, bool)),
				this,
				SLOT (handleViewLinkClicked (QUrl, bool)));

		TypeTimer_->setInterval (2000);
		connect (TypeTimer_,
				SIGNAL (timeout ()),
				this,
				SLOT (typeTimeout ()));

		PrepareTheme ();

		auto entry = GetEntry<ICLEntry> ();
		const int autoNum = XmlSettingsManager::Instance ()
				.property ("ShowLastNMessages").toInt ();
		if (entry->GetAllMessages ().size () <= 100 &&
				entry->GetEntryType () == ICLEntry::ETChat &&
				autoNum)
			RequestLogs (autoNum);

		InitEntry ();
		CheckMUC ();
		InitExtraActions ();
		InitMsgEdit ();
		RegisterSettings ();

		emit hookChatTabCreated (IHookProxy_ptr (new Util::DefaultHookProxy),
				this,
				GetEntry<QObject> (),
				Ui_.View_);

		Ui_.View_->setFocusProxy (Ui_.MsgEdit_);

		HandleMUCParticipantsChanged ();
	}

	ChatTab::~ChatTab ()
	{
		SetChatPartState (CPSGone);

		qDeleteAll (HistoryMessages_);
		delete Ui_.MsgEdit_->document ();

		delete MUCEventLog_;
	}

	void ChatTab::PrepareTheme ()
	{
		QString data = Core::Instance ().GetSelectedChatTemplate (GetEntry<QObject> (),
				Ui_.View_->page ()->mainFrame ());
		if (data.isEmpty ())
			data = "<h1 style='color:red;'>" +
					tr ("Unable to load style, "
						"please check you've enabled at least one styles plugin.") +
					"</h1>";
		Ui_.View_->setContent (data.toUtf8 (),
				"text/html", //"application/xhtml+xml" fails to work, though better to use it
				Core::Instance ().GetSelectedChatTemplateURL (GetEntry<QObject> ()));
	}

	void ChatTab::HasBeenAdded ()
	{
		UpdateStateIcon ();
	}

	TabClassInfo ChatTab::GetTabClassInfo () const
	{
		return IsMUC_ ? S_MUCTabClass_ : S_ChatTabClass_;
	}

	QList<QAction*> ChatTab::GetTabBarContextMenuActions () const
	{
		ActionsManager *manager = Core::Instance ().GetActionsManager ();
		QList<QAction*> allActions = manager->
				GetEntryActions (GetEntry<ICLEntry> ());
		QList<QAction*> result;
		Q_FOREACH (QAction *act, allActions)
		{
			if (manager->GetAreasForAction (act)
					.contains (ActionsManager::CLEAATabCtxtMenu) ||
				act->isSeparator ())
				result << act;
		}
		return result;
	}

	QObject* ChatTab::ParentMultiTabs ()
	{
		return S_ParentMultiTabs_;
	}

	QToolBar* ChatTab::GetToolBar () const
	{
		return TabToolbar_.get ();
	}

	void ChatTab::Remove ()
	{
		emit entryLostCurrent (GetEntry<QObject> ());
		emit needToClose (this);
	}

	void ChatTab::TabMadeCurrent ()
	{
		Core::Instance ().GetChatTabsManager ()->ChatMadeCurrent (this);
		Core::Instance ().FrameFocused (GetEntry<QObject> (), Ui_.View_->page ()->mainFrame ());

		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		emit hookMadeCurrent (proxy, this);
		if (proxy->IsCancelled ())
			return;

		emit entryMadeCurrent (GetEntry<QObject> ());

		NumUnreadMsgs_ = 0;
		HadHighlight_ = false;

		ReformatTitle ();
		Ui_.MsgEdit_->setFocus ();
	}

	void ChatTab::TabLostCurrent ()
	{
		TypeTimer_->stop ();
		SetChatPartState (CPSInactive);

		emit entryLostCurrent (GetEntry<QObject> ());
	}

	QByteArray ChatTab::GetTabRecoverData () const
	{
		QByteArray result;
		auto entry = GetEntry<ICLEntry> ();
		if (!entry)
			return result;

		QDataStream stream (&result, QIODevice::WriteOnly);
		if (entry->GetEntryType () == ICLEntry::ETMUC &&
				GetEntry<IMUCEntry> ())
			stream << QByteArray ("muctab2")
					<< entry->GetEntryID ()
					<< GetEntry<IMUCEntry> ()->GetIdentifyingData ()
					<< qobject_cast<IAccount*> (entry->GetParentAccount ())->GetAccountID ();
		else
			stream << QByteArray ("chattab2")
					<< entry->GetEntryID ()
					<< GetSelectedVariant ()
					<< Ui_.MsgEdit_->toPlainText ();

		return result;
	}

	QString ChatTab::GetTabRecoverName () const
	{
		auto entry = GetEntry<ICLEntry> ();
		return entry ?
				tr ("Chat with %1.")
					.arg (entry->GetEntryName ()) :
				GetTabClassInfo ().VisibleName_;
	}

	QIcon ChatTab::GetTabRecoverIcon () const
	{
		auto entry = GetEntry<ICLEntry> ();
		const auto& avatar = entry ? entry->GetAvatar () : QImage ();
		return avatar.isNull () ?
				GetTabClassInfo ().Icon_ :
				QPixmap::fromImage (avatar);
	}

	void ChatTab::FillMimeData (QMimeData *data)
	{
		if (auto entry = GetEntry<ICLEntry> ())
		{
			const auto& id = entry->GetHumanReadableID ();
			data->setText (id);
#if QT_VERSION >= 0x040800
			data->setUrls ({ id });
#endif
		}
	}

	void ChatTab::HandleDragEnter (QDragMoveEvent *event)
	{
		auto data = event->mimeData ();
		if (data->hasText ())
			event->acceptProposedAction ();
#if QT_VERSION >= 0x040800
		else if (data->hasUrls ())
		{
			for (const auto& url : data->urls ())
				if (url.isLocalFile () &&
						QFile::exists (url.toLocalFile ()))
				{
					event->acceptProposedAction ();
					break;
				}
		}
#endif
	}

	void ChatTab::HandleDrop (QDropEvent *event)
	{
		auto data = event->mimeData ();
		if (data->hasUrls ())
			handleFilesDropped (data->urls ());
		else if (data->hasText ())
			appendMessageText (data->text ());
	}

	void ChatTab::ShowUsersList ()
	{
		IMUCEntry *muc = GetEntry<IMUCEntry> ();
		if (!muc)
			return;

		const auto& parts = muc->GetParticipants ();
		UsersListWidget w (parts, this);
		if (w.exec () != QDialog::Accepted)
			return;

		if (auto part = w.GetActivatedParticipant ())
			InsertNick (qobject_cast<ICLEntry*> (part)->GetEntryName ());
	}

	void ChatTab::HandleMUCParticipantsChanged ()
	{
		IMUCEntry *muc = GetEntry<IMUCEntry> ();
		if (!muc)
			return;

		const int parts = muc->GetParticipants ().size ();
		const QString& text = tr ("%1 (%n participant(s))", 0, parts)
				.arg (GetEntry<ICLEntry> ()->GetEntryName ());
		Ui_.EntryInfo_->setText (text);
	}

	void ChatTab::SetEnabled (bool enabled)
	{
		auto children = findChildren<QWidget*> ();
		children += TabToolbar_.get ();
		children += MUCEventLog_;
		children += MsgFormatter_;
		Q_FOREACH (auto child, children)
			if (child != Ui_.View_)
				child->setEnabled (enabled);
	}

	QObject* ChatTab::GetCLEntry () const
	{
		return GetEntry<QObject> ();
	}

	QString ChatTab::GetSelectedVariant () const
	{
		return Ui_.VariantBox_->currentText ();
	}

	bool ChatTab::eventFilter (QObject *obj, QEvent *event)
	{
		if (obj == MUCEventLog_ && event->type () == QEvent::Close)
			Ui_.MUCEventsButton_->setChecked (false);

		return false;
	}

	void ChatTab::messageSend ()
	{
		QString text = Ui_.MsgEdit_->toPlainText ();
		if (text.isEmpty ())
			return;

		const QString& richText = MsgFormatter_->GetNormalizedRichText ();

		SetChatPartState (CPSActive);

		Ui_.MsgEdit_->clear ();
		Ui_.MsgEdit_->document ()->clear ();
		MsgFormatter_->Clear ();
		CurrentHistoryPosition_ = -1;
		MsgHistory_.prepend (text);

		QString variant = Ui_.VariantBox_->count () > 1 ?
				Ui_.VariantBox_->currentText () :
				QString ();

		ICLEntry *e = GetEntry<ICLEntry> ();
		IMessage::MessageType type =
				e->GetEntryType () == ICLEntry::ETMUC ?
						IMessage::MTMUCMessage :
						IMessage::MTChatMessage;

		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy ());
		proxy->SetValue ("text", text);
		emit hookMessageWillCreated (proxy, this, e->GetQObject (), type, variant);
		if (proxy->IsCancelled ())
			return;

		int intType = type;
		proxy->FillValue ("type", intType);
		type = static_cast<IMessage::MessageType> (intType);
		proxy->FillValue ("variant", variant);
		proxy->FillValue ("text", text);

		if (ProcessOutgoingMsg (e, text))
			return;

		QObject *msgObj = e->CreateMessage (type, variant, text);

		IMessage *msg = qobject_cast<IMessage*> (msgObj);
		if (!msg)
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to cast message from"
					<< e->GetEntryID ()
					<< "to IMessage"
					<< msgObj;
			return;
		}

		IRichTextMessage *richMsg = qobject_cast<IRichTextMessage*> (msgObj);
		if (richMsg &&
				!richText.isEmpty () &&
				ToggleRichText_->isChecked ())
			richMsg->SetRichBody (richText);

		proxy.reset (new Util::DefaultHookProxy ());
		emit hookMessageCreated (proxy, this, msg->GetQObject ());
		if (proxy->IsCancelled ())
			return;

		msg->Send ();
	}

	void ChatTab::on_MsgEdit__textChanged ()
	{
		UpdateTextHeight ();

		if (!Ui_.MsgEdit_->toPlainText ().isEmpty ())
		{
			SetChatPartState (CPSComposing);
			TypeTimer_->stop ();
			TypeTimer_->start ();
		}

		emit tabRecoverDataChanged ();
	}

	void ChatTab::on_SubjectButton__toggled (bool show)
	{
		Ui_.SubjBox_->setVisible (show);
		Ui_.SubjChange_->setEnabled (show);

		if (!show)
			return;

		IMUCEntry *me = GetEntry<IMUCEntry> ();
		if (!me)
			return;

		/* TODO enable depending on whether we have enough rights to
		 * change the subject. And, if we don't, set the SubjEdit_ to
		 * readOnly() mode.
		 */
		Ui_.SubjEdit_->setText (me->GetMUCSubject ());
	}

	void ChatTab::on_SubjChange__released()
	{
		Ui_.SubjectButton_->setChecked (false);

		IMUCEntry *me = GetEntry<IMUCEntry> ();
		if (!me)
			return;

		me->SetMUCSubject (Ui_.SubjEdit_->toPlainText ());
	}

	void ChatTab::on_View__loadFinished (bool)
	{
		Q_FOREACH (IMessage *msg, HistoryMessages_)
			AppendMessage (msg);

		ICLEntry *e = GetEntry<ICLEntry> ();
		if (!e)
		{
			qWarning () << Q_FUNC_INFO
					<< "null entry";
			return;
		}

		Q_FOREACH (QObject *msgObj, e->GetAllMessages ())
		{
			IMessage *msg = qobject_cast<IMessage*> (msgObj);
			if (!msg)
			{
				qWarning () << Q_FUNC_INFO
						<< "unable to cast message to IMessage"
						<< msgObj;
				continue;
			}
			AppendMessage (msg);
		}

		QFile scrollerJS (":/plugins/azoth/resources/scripts/scrollers.js");
		if (!scrollerJS.open (QIODevice::ReadOnly))
			qWarning () << Q_FUNC_INFO
					<< "unable to open script file"
					<< scrollerJS.errorString ();
		else
		{
			Ui_.View_->page ()->mainFrame ()->evaluateJavaScript (scrollerJS.readAll ());
			Ui_.View_->page ()->mainFrame ()->evaluateJavaScript ("InstallEventListeners(); ScrollToBottom();");
		}

		emit hookThemeReloaded (Util::DefaultHookProxy_ptr (new Util::DefaultHookProxy),
				this, Ui_.View_, GetEntry<QObject> ());
	}

#ifdef ENABLE_MEDIACALLS
	void ChatTab::handleCallRequested ()
	{
		QObject *callObj = Core::Instance ().GetCallManager ()->
				Call (GetEntry<ICLEntry> (), Ui_.VariantBox_->currentText ());
		if (!callObj)
			return;
		handleCall (callObj);
	}

	void ChatTab::handleCall (QObject *callObj)
	{
		IMediaCall *call = qobject_cast<IMediaCall*> (callObj);
		if (!call || call->GetSourceID () != EntryID_)
			return;

		CallChatWidget *widget = new CallChatWidget (callObj);
		const int idx = Ui_.MainLayout_->indexOf (Ui_.View_);
		Ui_.MainLayout_->insertWidget (idx, widget);
	}
#endif

#ifdef ENABLE_CRYPT
	void ChatTab::handleEnableEncryption ()
	{
		QObject *accObj = GetEntry<ICLEntry> ()->GetParentAccount ();
		ISupportPGP *pgp = qobject_cast<ISupportPGP*> (accObj);
		if (!pgp)
		{
			qWarning () << Q_FUNC_INFO
					<< accObj
					<< "doesn't implement ISupportPGP";
			return;
		}

		const bool enable = EnableEncryption_->isChecked ();

		EnableEncryption_->blockSignals (true);
		EnableEncryption_->setChecked (!enable);
		EnableEncryption_->blockSignals (false);

		pgp->SetEncryptionEnabled (GetEntry<QObject> (), enable);
	}

	void ChatTab::handleEncryptionStateChanged (QObject *entry, bool enabled)
	{
		if (entry != GetEntry<QObject> ())
			return;

		EnableEncryption_->blockSignals (true);
		EnableEncryption_->setChecked (enabled);
		EnableEncryption_->blockSignals (false);
	}
#endif

	void ChatTab::handleClearChat ()
	{
		ICLEntry *entry = GetEntry<ICLEntry> ();
		if (!entry)
			return;

		ScrollbackPos_ = 0;
		entry->PurgeMessages (QDateTime ());
		qDeleteAll (HistoryMessages_);
		HistoryMessages_.clear ();
		PrepareTheme ();
	}

	void ChatTab::handleHistoryBack ()
	{
		ScrollbackPos_ += 50;
		qDeleteAll (HistoryMessages_);
		HistoryMessages_.clear ();
		RequestLogs (ScrollbackPos_);
	}

	void ChatTab::handleRichTextToggled ()
	{
		PrepareTheme ();

		const bool defaultSetting = XmlSettingsManager::Instance ()
				.property ("ShowRichTextMessageBody").toBool ();

		QSettings settings (QCoreApplication::organizationName (),
				QCoreApplication::applicationName () + "_Azoth");
		settings.beginGroup ("RichTextStates");

		auto enabled = settings.value ("Enabled").toStringList ();
		auto disabled = settings.value ("Disabled").toStringList ();

		if (ToggleRichText_->isChecked () == defaultSetting)
		{
			enabled.removeAll (EntryID_);
			disabled.removeAll (EntryID_);
		}
		else if (defaultSetting)
			disabled << EntryID_;
		else
			enabled << EntryID_;

		settings.setValue ("Enabled", enabled);
		settings.setValue ("Disabled", disabled);

		settings.endGroup ();
	}

	void ChatTab::handleQuoteSelection ()
	{
		const QString& selected = Ui_.View_->selectedText ();
		if (selected.isEmpty ())
			return;

		QStringList split = selected.split ('\n');
		for (int i = 0; i < split.size (); ++i)
			split [i].prepend ("> ");

		split << QString ();

		const QString& toInsert = split.join ("\n");
		QTextCursor cur = Ui_.MsgEdit_->textCursor ();
		cur.insertText (toInsert);
	}

	void ChatTab::handleOpenLastLink ()
	{
		if (LastLink_.isEmpty ())
			return;

		const auto& e = Util::MakeEntity (QUrl (LastLink_), QString (), static_cast<TaskParameters> (FromUserInitiated | OnlyHandle));
		Core::Instance ().GetProxy ()->GetEntityManager ()->HandleEntity (e);
	}

	void ChatTab::handleFileOffered (QObject *jobObj)
	{
		ITransferJob *job = qobject_cast<ITransferJob*> (jobObj);
		if (!job)
		{
			qWarning () << Q_FUNC_INFO
					<< jobObj
					<< "could not be casted to ITransferJob";
			return;
		}

		if (job->GetSourceID () != EntryID_)
			return;

		Ui_.EventsButton_->show ();

		const QString& text = tr ("File offered: %1.")
				.arg (job->GetName ());
		QAction *act = Ui_.EventsButton_->menu ()->
				addAction (text, this, SLOT (handleOfferActionTriggered ()));
		act->setData (QVariant::fromValue<QObject*> (jobObj));
		act->setToolTip (job->GetComment ());
	}

	void ChatTab::handleFileNoLongerOffered (QObject *jobObj)
	{
		Q_FOREACH (QAction *action, Ui_.EventsButton_->menu ()->actions ())
			if (action->data ().value<QObject*> () == jobObj)
			{
				action->deleteLater ();
				break;
			}

		if (Ui_.EventsButton_->menu ()->actions ().count () == 1)
			Ui_.EventsButton_->hide ();
	}

	void ChatTab::handleOfferActionTriggered ()
	{
		QAction *action = qobject_cast<QAction*> (sender ());
		if (!action)
		{
			qWarning () << Q_FUNC_INFO
					<< sender ()
					<< "is not a QAction";
			return;
		}

		QObject *jobObj = action->data ().value<QObject*> ();
		ITransferJob *job = qobject_cast<ITransferJob*> (jobObj);

		QString text = tr ("Would you like to accept or reject file transfer "
				"request for file %1?")
					.arg (job->GetName ());
		if (!job->GetComment ().isEmpty ())
		{
			text += "<br /><br />" + tr ("The file description is:") + "<br /><br /><em>";
			auto comment = Qt::escape (job->GetComment ());
			comment.replace ("\n", "<br />");
			text += comment + "</em>";
		}

		auto questResult = QMessageBox::question (this,
				tr ("File transfer request"), text,
				QMessageBox::Save | QMessageBox::Abort | QMessageBox::Cancel);

		if (questResult == QMessageBox::Cancel)
			return;
		else if (questResult == QMessageBox::Abort)
			Core::Instance ().GetTransferJobManager ()->DenyJob (jobObj);
		else
		{
			const QString& path = QFileDialog::getExistingDirectory (this,
					tr ("Select save path for incoming file"),
					XmlSettingsManager::Instance ()
							.property ("DefaultXferSavePath").toString ());
			if (path.isEmpty ())
				return;

			Core::Instance ().GetTransferJobManager ()->AcceptJob (jobObj, path);
		}

		action->deleteLater ();

		if (Ui_.EventsButton_->menu ()->actions ().size () == 1)
			Ui_.EventsButton_->hide ();
	}

	void ChatTab::handleEntryMessage (QObject *msgObj)
	{
		IMessage *msg = qobject_cast<IMessage*> (msgObj);
		if (!msg)
		{
			qWarning () << Q_FUNC_INFO
					<< msgObj
					<< "doesn't implement IMessage"
					<< sender ();
			return;
		}

		auto entry = GetEntry<ICLEntry> ();
		bool shouldReformat = false;
		if (Core::Instance ().ShouldCountUnread (entry, msg))
		{
			++NumUnreadMsgs_;
			shouldReformat = true;
		}
		else
			GetEntry<ICLEntry> ()->MarkMsgsRead ();

		if (msg->GetMessageType () == IMessage::MTMUCMessage &&
				!Core::Instance ().GetChatTabsManager ()->IsActiveChat (entry) &&
				!HadHighlight_)
		{
			HadHighlight_ = Core::Instance ().IsHighlightMessage (msg);
			if (HadHighlight_)
				shouldReformat = true;
		}

		if (shouldReformat)
			ReformatTitle ();

		if (msg->GetMessageType () == IMessage::MTChatMessage &&
				msg->GetDirection () == IMessage::DIn)
		{
			const int idx = Ui_.VariantBox_->findText (msg->GetOtherVariant ());
			if (idx != -1)
				Ui_.VariantBox_->setCurrentIndex (idx);
		}

		AppendMessage (msg);
	}

	void ChatTab::handleVariantsChanged (QStringList variants)
	{
		if (!variants.isEmpty () &&
				!variants.contains (QString ()))
			variants.prepend (QString ());

		if (variants.size () == Ui_.VariantBox_->count ())
		{
			bool samelist = true;
			for (int i = 0, size = variants.size (); i < size; ++i)
				if (variants.at (i) != Ui_.VariantBox_->itemText (i))
				{
					samelist = false;
					break;
				}

			if (samelist)
				return;
		}

		const QString& current = Ui_.VariantBox_->currentText ();
		Ui_.VariantBox_->clear ();

		Q_FOREACH (const QString& variant, variants)
		{
			const State& st = GetEntry<ICLEntry> ()->GetStatus (variant).State_;
			const QIcon& icon = Core::Instance ().GetIconForState (st);
			Ui_.VariantBox_->addItem (icon, variant);
		}

		if (!variants.isEmpty ())
		{
			const int pos = std::max (0, Ui_.VariantBox_->findText (current));
			Ui_.VariantBox_->setCurrentIndex (pos);
		}

		Ui_.VariantBox_->setVisible (variants.size () > 1);
	}

	void ChatTab::handleAvatarChanged (const QImage& avatar)
	{
		const QPixmap& px = QPixmap::fromImage (avatar);
		if (!px.isNull ())
		{
			const QPixmap& scaled = px.scaled (QSize (18, 18), Qt::KeepAspectRatio, Qt::SmoothTransformation);
			Ui_.AvatarLabel_->setPixmap (scaled);
			Ui_.AvatarLabel_->resize (scaled.size ());
			Ui_.AvatarLabel_->setMaximumSize (scaled.size ());
		}
		Ui_.AvatarLabel_->setVisible (!avatar.isNull ());
	}

	void ChatTab::handleStatusChanged (const EntryStatus& status,
			const QString& variant)
	{
		auto entry = GetEntry<ICLEntry> ();
		if (entry->GetEntryType () == ICLEntry::ETMUC)
			return;

		const QStringList& vars = entry->Variants ();
		handleVariantsChanged (vars);

		if (vars.value (0) == variant ||
				variant.isEmpty () ||
				vars.isEmpty ())
		{
			const QIcon& icon = Core::Instance ().GetIconForState (status.State_);
			TabIcon_ = icon;
			UpdateStateIcon ();
		}
	}

	void ChatTab::handleChatPartStateChanged (const ChatPartState& state, const QString&)
	{
		QString text = GetEntry<ICLEntry> ()->GetEntryName ();

		QString chatState;
		switch (state)
		{
		case CPSActive:
			chatState = tr ("participating");
			break;
		case CPSInactive:
			chatState = tr ("inactive");
			break;
		case CPSComposing:
			chatState = tr ("composing");
			break;
		case CPSPaused:
			chatState = tr ("paused composing");
			break;
		case CPSGone:
			chatState = tr ("left the conversation");
			break;
		default:
			break;
		}
		if (!chatState.isEmpty ())
			text += " (" + chatState + ")";

		Ui_.EntryInfo_->setText (text);
	}

	namespace
	{
		void OpenChatWithText (QUrl newUrl, const QString& id, ICLEntry *own)
		{
			newUrl.removeQueryItem ("hrid");

			IAccount *account = qobject_cast<IAccount*> (own->GetParentAccount ());
			Q_FOREACH (QObject *entryObj, account->GetCLEntries ())
			{
				ICLEntry *entry = qobject_cast<ICLEntry*> (entryObj);
				if (!entry || entry->GetHumanReadableID () != id)
					continue;

				QWidget *w = Core::Instance ()
						.GetChatTabsManager ()->OpenChat (entry);
				QMetaObject::invokeMethod (w,
						"handleViewLinkClicked",
						Qt::QueuedConnection,
						Q_ARG (QUrl, newUrl));
				break;
			}
		}
	}

	void ChatTab::handleViewLinkClicked (QUrl url, bool raise)
	{
		if (url.scheme () != "azoth")
		{
			if (Core::Instance ().CouldHandleURL (url))
			{
				Core::Instance ().HandleURL (url, GetEntry<ICLEntry> ());
				return;
			}

			if (url.scheme ().isEmpty () &&
					url.host ().isEmpty () &&
					url.path ().startsWith ("www."))
				url = "http://" + url.toString ();

			Entity e = Util::MakeEntity (url,
					QString (),
					static_cast<TaskParameter> (FromUserInitiated | OnlyHandle | ShouldQuerySource));
			if (!raise)
				e.Additional_ ["BackgroundHandle"] = true;
			Core::Instance ().SendEntity (e);
			return;
		}

		const auto& host = url.host ();
		if (host == "msgeditreplace")
		{
			if (url.queryItems ().isEmpty ())
			{
				Ui_.MsgEdit_->setText (url.path ().mid (1));
				Ui_.MsgEdit_->moveCursor (QTextCursor::End);
				Ui_.MsgEdit_->setFocus ();
			}
			else
				Q_FOREACH (auto item, url.queryItems ())
					if (item.first == "hrid")
					{
						OpenChatWithText (url, item.second, GetEntry<ICLEntry> ());
						return;
					}
		}
		else if (host == "msgeditinsert")
		{
			const auto& text = url.path ().mid (1);
			const auto& split = text.split ("/#/", QString::SkipEmptyParts);

			const auto& insertText = split.value (0);
			const auto& replaceText = split.size () > 1 ?
					split.value (1) :
					insertText;

			if (Ui_.MsgEdit_->toPlainText ().simplified ().trimmed ().isEmpty ())
			{
				Ui_.MsgEdit_->setText (replaceText);
				Ui_.MsgEdit_->moveCursor (QTextCursor::End);
				Ui_.MsgEdit_->setFocus ();
			}
			else
				Ui_.MsgEdit_->textCursor ().insertText (insertText);
		}
		else if (host == "insertnick")
		{
			const auto& encoded = url.encodedQueryItemValue ("nick");
			const auto& nick = QUrl::fromPercentEncoding (encoded);
			InsertNick (nick);

			if (!GetMUCParticipants ().contains (nick))
				Core::Instance ().SendEntity (Util::MakeNotification ("Azoth",
							tr ("%1 isn't present in this conference at the moment.")
								.arg ("<em>" + nick + "</em>"),
							PWarning_));
		}
	}

	void ChatTab::InsertNick (const QString& nicknameHtml)
	{
		const QString& post = XmlSettingsManager::Instance ()
				.property ("PostAddressText").toString ();
		QTextCursor cursor = Ui_.MsgEdit_->textCursor ();
		cursor.insertHtml (cursor.atStart () ?
				nicknameHtml + post + " " :
				" &nbsp;" + nicknameHtml + " ");
		Ui_.MsgEdit_->setFocus ();
	}

	void ChatTab::handleHistoryUp ()
	{
		if (CurrentHistoryPosition_ == MsgHistory_.size () - 1)
			return;

		Ui_.MsgEdit_->setText (MsgHistory_
					.at (++CurrentHistoryPosition_));
	}

	void ChatTab::handleHistoryDown ()
	{
		if (CurrentHistoryPosition_ == -1)
			return;

		if (CurrentHistoryPosition_-- == 0)
			Ui_.MsgEdit_->clear ();
		else
			Ui_.MsgEdit_->setText (MsgHistory_
						.at (CurrentHistoryPosition_));

		Ui_.MsgEdit_->moveCursor (QTextCursor::End);
	}

	void ChatTab::typeTimeout ()
	{
		SetChatPartState (CPSPaused);
	}

	void ChatTab::handleLocalImageDropped (const QImage& image, const QUrl& url)
	{
		if (url.scheme () == "file")
			handleFilesDropped (QList<QUrl> () << url);
		else
		{
			if (QMessageBox::question (this,
						"Sending image",
						tr ("Would you like to send image %1 directly in chat? "
							"Otherwise the link to it will be sent.")
							.arg (QFileInfo (url.path ()).fileName ()),
						QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
			{
				handleImageDropped (image);
				return;
			}

			auto entry = GetEntry<ICLEntry> ();
			if (!entry)
				return;

			const auto msgType = entry->GetEntryType () == ICLEntry::ETMUC ?
						IMessage::MTMUCMessage :
						IMessage::MTChatMessage;
			auto msgObj = entry->CreateMessage (msgType, GetSelectedVariant (), url.toEncoded ());
			auto msg = qobject_cast<IMessage*> (msgObj);
			msg->Send ();
		}
	}

	void ChatTab::handleImageDropped (const QImage& image)
	{
		auto entry = GetEntry<ICLEntry> ();
		if (!entry)
			return;

		const auto msgType = entry->GetEntryType () == ICLEntry::ETMUC ?
					IMessage::MTMUCMessage :
					IMessage::MTChatMessage;
		auto msgObj = entry->CreateMessage (msgType,
				GetSelectedVariant (),
				tr ("This message contains inline image, enable XHTML-IM to view it."));
		auto msg = qobject_cast<IMessage*> (msgObj);

		if (IRichTextMessage *richMsg = qobject_cast<IRichTextMessage*> (msgObj))
		{
			QString asBase;
			if (entry->GetEntryType () == ICLEntry::ETMUC)
			{
				QBuffer buf;
				buf.open (QIODevice::ReadWrite);
				image.save (&buf, "JPG", 60);
				asBase = QString ("data:image/png;base64,%1")
						.arg (QString (buf.buffer ().toBase64 ()));
			}
			else
				asBase = Util::GetAsBase64Src (image);
			const auto& body = "<img src='" + asBase + "'/>";
			richMsg->SetRichBody (body);
		}

		msg->Send ();
	}

	namespace
	{
		bool CheckImage (const QList<QUrl>& urls, ChatTab *chat)
		{
			if (urls.size () != 1)
				return false;

			const auto& local = urls.at (0).toLocalFile ();
			if (!QFile::exists (local))
				return false;

			const QImage img (local);
			if (img.isNull ())
				return false;

			const QFileInfo fileInfo (local);

			if (QMessageBox::question (chat,
						"Sending image",
						ChatTab::tr ("Would you like to send image %1 (%2) directly in chat? "
							"Otherwise it will be sent as file.")
							.arg (fileInfo.fileName ())
							.arg (Util::MakePrettySize (fileInfo.size ())),
						QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
				return false;

			chat->handleImageDropped (img);
			return true;
		}
	}

	void ChatTab::handleFilesDropped (const QList<QUrl>& urls)
	{
		if (CheckImage (urls, this))
			return;

		Core::Instance ().GetTransferJobManager ()->OfferURLs (GetEntry<ICLEntry> (), urls);
	}

	void ChatTab::handleGotLastMessages (QObject *entryObj, const QList<QObject*>& messages)
	{
		if (entryObj != GetEntry<QObject> ())
			return;

		ICLEntry *entry = GetEntry<ICLEntry> ();
		QList<QObject*> rMsgs = entry->GetAllMessages ();
		std::reverse (rMsgs.begin (), rMsgs.end ());

		Q_FOREACH (QObject *msgObj, messages)
		{
			IMessage *msg = qobject_cast<IMessage*> (msgObj);
			if (!msg)
			{
				qWarning () << Q_FUNC_INFO
						<< msgObj
						<< "doesn't implement IMessage";
				continue;
			}

			const QDateTime& dt = msg->GetDateTime ();

			if (std::find_if (rMsgs.begin (), rMsgs.end (),
					[msg] (QObject *msgObj) -> bool
					{
						IMessage *tMsg = qobject_cast<IMessage*> (msgObj);
						if (!tMsg)
							return false;
						return tMsg->GetDirection () == msg->GetDirection () &&
								tMsg->GetBody () == msg->GetBody () &&
								std::abs (tMsg->GetDateTime ().secsTo (msg->GetDateTime ())) < 5;
					}) != rMsgs.end ())
				continue;

			if (HistoryMessages_.isEmpty () ||
					HistoryMessages_.last ()->GetDateTime () <= dt)
				HistoryMessages_ << msg;
			else
			{
				auto pos = std::find_if (HistoryMessages_.begin (), HistoryMessages_.end (),
						[dt] (IMessage *msg) { return msg->GetDateTime () > dt; });
				HistoryMessages_.insert (pos, msg);
			}
		}

		if (!messages.isEmpty ())
			PrepareTheme ();

		disconnect (sender (),
				SIGNAL (gotLastMessages (QObject*, const QList<QObject*>&)),
				this,
				SLOT (handleGotLastMessages (QObject*, const QList<QObject*>&)));
	}

	void ChatTab::handleSendButtonVisible ()
	{
		Ui_.SendButton_->setVisible (XmlSettingsManager::Instance ()
					.property ("SendButtonVisible").toBool ());
	}

	void ChatTab::handleMinLinesHeightChanged ()
	{
		PreviousTextHeight_ = 0;
		UpdateTextHeight ();
	}

	void ChatTab::handleRichFormatterPosition ()
	{
		const QString& posStr = XmlSettingsManager::Instance ()
				.property ("RichFormatterPosition").toString ();
		const int pos = Ui_.MainLayout_->indexOf (Ui_.View_) + (posStr == "belowEdit" ? 2 : 1);
		Ui_.MainLayout_->insertWidget (pos, MsgFormatter_);
	}

	void ChatTab::handleFontSettingsChanged ()
	{
		QWebSettings *s = Ui_.View_->settings ();

		auto font = [s] (QWebSettings::FontFamily f, const QByteArray& str)
		{
			const QString& family = XmlSettingsManager::Instance ()
					.property (str).value<QFont> ().family ();
			s->setFontFamily (f, family);
		};

		font (QWebSettings::StandardFont, "StandardFont");
		font (QWebSettings::FixedFont, "FixedFont");
		font (QWebSettings::SerifFont, "SerifFont");
		font (QWebSettings::SansSerifFont, "SansSerifFont");
		font (QWebSettings::CursiveFont, "CursiveFont");
		font (QWebSettings::FantasyFont, "FantasyFont");
	}

	void ChatTab::handleFontSizeChanged ()
	{
		const int size = XmlSettingsManager::Instance ()
				.property ("FontSize").toInt ();
		Ui_.View_->settings ()->setFontSize (QWebSettings::DefaultFontSize, size);
	}

	template<typename T>
	T* ChatTab::GetEntry () const
	{
		QObject *obj = Core::Instance ().GetEntry (EntryID_);
		T *entry = qobject_cast<T*> (obj);
		if (!entry)
			qWarning () << Q_FUNC_INFO
					<< "object"
					<< obj
					<< "doesn't implement the required interface";
		return entry;
	}

	void ChatTab::BuildBasicActions ()
	{
		auto sm = Core::Instance ().GetShortcutManager ();
		const auto& infos = sm->GetActionInfo ();

		const auto& clearInfo = infos ["org.LeechCraft.Azoth.ClearChat"];
		QAction *clearAction = new QAction (clearInfo.UserVisibleText_, this);
		clearAction->setProperty ("ActionIcon", "edit-clear-history");
		clearAction->setShortcuts (clearInfo.Seqs_);
		connect (clearAction,
				SIGNAL (triggered ()),
				this,
				SLOT (handleClearChat ()));
		TabToolbar_->addAction (clearAction);
		sm->RegisterAction ("org.LeechCraft.Azoth.ClearChat", clearAction, true);

		const auto& backInfo = infos ["org.LeechCraft.Azoth.ScrollHistoryBack"];
		QAction *historyBack = new QAction (backInfo.UserVisibleText_, this);
		historyBack->setProperty ("ActionIcon", "go-previous");
		historyBack->setShortcuts (backInfo.Seqs_);
		connect (historyBack,
				SIGNAL (triggered ()),
				this,
				SLOT (handleHistoryBack ()));
		TabToolbar_->addAction (historyBack);
		sm->RegisterAction ("org.LeechCraft.Azoth.ScrollHistoryBack", historyBack, true);

		TabToolbar_->addSeparator ();

		ToggleRichText_ = new QAction (tr ("Enable rich text"), this);
		ToggleRichText_->setProperty ("ActionIcon", "text-enriched");
		ToggleRichText_->setCheckable (true);
		ToggleRichText_->setChecked (XmlSettingsManager::Instance ()
					.property ("ShowRichTextMessageBody").toBool ());
		connect (ToggleRichText_,
				SIGNAL (toggled (bool)),
				this,
				SLOT (handleRichTextToggled ()));
		TabToolbar_->addAction (ToggleRichText_);
		TabToolbar_->addSeparator ();

		QSettings settings (QCoreApplication::organizationName (),
				QCoreApplication::applicationName () + "_Azoth");
		settings.beginGroup ("RichTextStates");
		if (settings.value ("Enabled").toStringList ().contains (EntryID_))
			ToggleRichText_->setChecked (true);
		else if (settings.value ("Disabled").toStringList ().contains (EntryID_))
			ToggleRichText_->setChecked (false);
		settings.endGroup ();

		const auto& quoteInfo = infos ["org.LeechCraft.Azoth.QuoteSelected"];
		QAction *quoteSelection = new QAction (tr ("Quote selection"), this);
		quoteSelection->setProperty ("ActionIcon", "mail-reply-sender");
		quoteSelection->setShortcuts (quoteInfo.Seqs_);
		connect (quoteSelection,
				SIGNAL (triggered ()),
				this,
				SLOT (handleQuoteSelection ()));
		TabToolbar_->addAction (quoteSelection);
		TabToolbar_->addSeparator ();
		sm->RegisterAction ("org.LeechCraft.Azoth.QuoteSelected", quoteSelection, true);

		Ui_.View_->SetQuoteAction (quoteSelection);

		const auto& openLinkInfo = infos ["org.LeechCraft.Azoth.OpenLastLink"];
		auto shortcut = new QShortcut (openLinkInfo.Seqs_.value (0),
				this, SLOT (handleOpenLastLink ()), 0, Qt::WidgetWithChildrenShortcut);
		sm->RegisterShortcut ("org.LeechCraft.Azoth.OpenLastLink", openLinkInfo, shortcut);
	}

	void ChatTab::InitEntry ()
	{
		connect (GetEntry<QObject> (),
				SIGNAL (gotMessage (QObject*)),
				this,
				SLOT (handleEntryMessage (QObject*)));
		connect (GetEntry<QObject> (),
				SIGNAL (statusChanged (const EntryStatus&, const QString&)),
				this,
				SLOT (handleStatusChanged (const EntryStatus&, const QString&)));
		connect (GetEntry<QObject> (),
				SIGNAL (availableVariantsChanged (const QStringList&)),
				this,
				SLOT (handleVariantsChanged (QStringList)));
		connect (GetEntry<QObject> (),
				SIGNAL (avatarChanged (const QImage&)),
				this,
				SLOT (handleAvatarChanged (const QImage&)));

		ICLEntry *e = GetEntry<ICLEntry> ();
		handleVariantsChanged (e->Variants ());
		handleAvatarChanged (e->GetAvatar ());
		Ui_.EntryInfo_->setText (e->GetEntryName ());

		const QString& accName =
				qobject_cast<IAccount*> (e->GetParentAccount ())->
						GetAccountName ();
		Ui_.AccountName_->setText (accName);
	}

	void ChatTab::CheckMUC ()
	{
		ICLEntry *e = GetEntry<ICLEntry> ();

		bool claimsMUC = e->GetEntryType () == ICLEntry::ETMUC;
		IsMUC_ = true;
		if (!claimsMUC)
			IsMUC_ = false;

		if (claimsMUC &&
				!GetEntry<IMUCEntry> ())
		{
			qWarning () << Q_FUNC_INFO
				<< e->GetEntryName ()
				<< "declares itself to be a MUC, "
					"but doesn't implement IMUCEntry";
			IsMUC_  = false;
		}

		if (IsMUC_ )
			HandleMUC ();
		else
		{
			Ui_.SubjectButton_->hide ();
			Ui_.MUCEventsButton_->hide ();
			TabIcon_ = Core::Instance ()
					.GetIconForState (e->GetStatus ().State_);

			connect (GetEntry<QObject> (),
					SIGNAL (chatPartStateChanged (const ChatPartState&, const QString&)),
					this,
					SLOT (handleChatPartStateChanged (const ChatPartState&, const QString&)));
		}
	}

	void ChatTab::HandleMUC ()
	{
		TabIcon_ = QIcon (":/plugins/azoth/resources/images/azoth.svg");
		Ui_.AvatarLabel_->hide ();

		const int height = qApp->desktop ()->availableGeometry (QCursor::pos ()).height ();

		MUCEventLog_->setWindowTitle (tr ("MUC log for %1")
					.arg (GetEntry<ICLEntry> ()->GetHumanReadableID ()));
		MUCEventLog_->setStyleSheet ("background-color: rgb(0, 0, 0);");
		MUCEventLog_->resize (600, height * 2 / 3);

		XmlSettingsManager::Instance ().RegisterObject ("SeparateMUCEventLogWindow",
				this, "handleSeparateMUCLog");
		handleSeparateMUCLog ();
	}

	void ChatTab::InitExtraActions ()
	{
		ICLEntry *e = GetEntry<ICLEntry> ();
		QObject *accObj = e->GetParentAccount ();
		IAccount *acc = qobject_cast<IAccount*> (accObj);
		if (qobject_cast<ITransferManager*> (acc->GetTransferManager ()))
		{
			connect (acc->GetTransferManager (),
					SIGNAL (fileOffered (QObject*)),
					this,
					SLOT (handleFileOffered (QObject*)));

			Q_FOREACH (QObject *object,
					Core::Instance ().GetTransferJobManager ()->
							GetPendingIncomingJobsFor (EntryID_))
				handleFileOffered (object);
		}

#ifdef ENABLE_MEDIACALLS
		if (qobject_cast<ISupportMediaCalls*> (accObj) &&
				e->GetEntryType () == ICLEntry::ETChat)
		{
			Call_ = new QAction (tr ("Call..."), this);
			Call_->setProperty ("ActionIcon", "voicecall");
			connect (Call_,
					SIGNAL (triggered ()),
					this,
					SLOT (handleCallRequested ()));
			TabToolbar_->addAction (Call_);

			connect (accObj,
					SIGNAL (called (QObject*)),
					this,
					SLOT (handleCall (QObject*)));

			Q_FOREACH (QObject *object,
					Core::Instance ().GetCallManager ()->
							GetCallsForEntry (EntryID_))
				handleCall (object);
		}
#endif

#ifdef ENABLE_CRYPT
		if (qobject_cast<ISupportPGP*> (accObj))
		{
			EnableEncryption_ = new QAction (tr ("Enable encryption"), this);
			EnableEncryption_->setProperty ("ActionIcon", "document-encrypt");
			EnableEncryption_->setCheckable (true);
			connect (EnableEncryption_,
					SIGNAL (triggered ()),
					this,
					SLOT (handleEnableEncryption ()));
			TabToolbar_->addAction (EnableEncryption_);

			connect (accObj,
					SIGNAL (encryptionStateChanged (QObject*, bool)),
					this,
					SLOT (handleEncryptionStateChanged (QObject*, bool)));
		}
#endif

		QList<QAction*> coreActions;
		ActionsManager *manager = Core::Instance ().GetActionsManager ();
		Q_FOREACH (QAction *action, manager->GetEntryActions (e))
			if (manager->GetAreasForAction (action).contains (ActionsManager::CLEAAToolbar))
				coreActions << action;

		if (!coreActions.isEmpty ())
		{
			TabToolbar_->addSeparator ();
			TabToolbar_->addActions (coreActions);
		}
	}

	void ChatTab::InitMsgEdit ()
	{
#ifndef Q_OS_MAC
		const auto histModifier = Qt::CTRL;
#else
		const auto histModifier = Qt::ALT;
#endif
		QShortcut *histUp = new QShortcut (histModifier + Qt::Key_Up,
				Ui_.MsgEdit_, 0, 0, Qt::WidgetShortcut);
		connect (histUp,
				SIGNAL (activated ()),
				this,
				SLOT (handleHistoryUp ()));

		QShortcut *histDown = new QShortcut (histModifier + Qt::Key_Down,
				Ui_.MsgEdit_, 0, 0, Qt::WidgetShortcut);
		connect (histDown,
				SIGNAL (activated ()),
				this,
				SLOT (handleHistoryDown ()));

		connect (Ui_.MsgEdit_,
				SIGNAL (keyReturnPressed ()),
				this,
				SLOT (messageSend ()));
		connect (Ui_.MsgEdit_,
				SIGNAL (keyTabPressed ()),
				this,
				SLOT (nickComplete ()));
		connect (Ui_.MsgEdit_,
				SIGNAL (scroll (int)),
				this,
				SLOT (handleEditScroll (int)));

		QTimer::singleShot (0,
				Ui_.MsgEdit_,
				SLOT (setFocus ()));

		connect (Ui_.MsgEdit_,
				SIGNAL (clearAvailableNicks ()),
				this,
				SLOT (clearAvailableNick ()));
		UpdateTextHeight ();

		MsgFormatter_ = new MsgFormatterWidget (Ui_.MsgEdit_, Ui_.MsgEdit_);
		handleRichFormatterPosition ();
		connect (ToggleRichText_,
				SIGNAL (toggled (bool)),
				MsgFormatter_,
				SLOT (setVisible (bool)));
		MsgFormatter_->setVisible (ToggleRichText_->isChecked ());
	}

	void ChatTab::RegisterSettings()
	{
		XmlSettingsManager::Instance ().RegisterObject ("FontSize",
				this, "handleFontSizeChanged");
		handleFontSizeChanged ();

		QList<QByteArray> fontProps;
		fontProps << "StandardFont"
				<< "FixedFont"
				<< "SerifFont"
				<< "SansSerifFont"
				<< "CursiveFont"
				<< "FantasyFont";
		XmlSettingsManager::Instance ().RegisterObject (fontProps,
				this, "handleFontSettingsChanged");
		handleFontSettingsChanged ();

		XmlSettingsManager::Instance ().RegisterObject ("RichFormatterPosition",
				this, "handleRichFormatterPosition");

		XmlSettingsManager::Instance ().RegisterObject ("SendButtonVisible",
				this, "handleSendButtonVisible");
		handleSendButtonVisible ();

		XmlSettingsManager::Instance ().RegisterObject ("MinLinesHeight",
				this, "handleMinLinesHeightChanged");
	}

	void ChatTab::RequestLogs (int num)
	{
		ICLEntry *entry = GetEntry<ICLEntry> ();
		if (!entry)
		{
			qWarning () << Q_FUNC_INFO
					<< "null entry for"
					<< EntryID_;
			return;
		}

		QObject *entryObj = entry->GetQObject ();

		const QObjectList& histories = Core::Instance ().GetProxy ()->
				GetPluginsManager ()->GetAllCastableRoots<IHistoryPlugin*> ();

		Q_FOREACH (QObject *histObj, histories)
		{
			IHistoryPlugin *hist = qobject_cast<IHistoryPlugin*> (histObj);
			if (!hist->IsHistoryEnabledFor (entryObj))
				continue;

			connect (histObj,
					SIGNAL (gotLastMessages (QObject*, const QList<QObject*>&)),
					this,
					SLOT (handleGotLastMessages (QObject*, const QList<QObject*>&)),
					Qt::UniqueConnection);

			hist->RequestLastMessages (entryObj, num);
		}
	}

	void ChatTab::AppendMessage (IMessage *msg)
	{
		ICLEntry *other = qobject_cast<ICLEntry*> (msg->OtherPart ());
		if (!other && msg->OtherPart ())
		{
			qWarning () << Q_FUNC_INFO
					<< "message's other part doesn't implement ICLEntry"
					<< msg->GetQObject ()
					<< msg->OtherPart ();
			return;
		}

		if (msg->GetQObject ()->property ("Azoth/HiddenMessage").toBool ())
			return;

		ICLEntry *parent = qobject_cast<ICLEntry*> (msg->ParentCLEntry ());

		if (msg->GetDirection () == IMessage::DOut &&
				other->GetEntryType () == ICLEntry::ETMUC)
			return;

		if (msg->GetMessageSubType () == IMessage::MSTParticipantStatusChange &&
				(!parent || parent->GetEntryType () == ICLEntry::ETMUC) &&
				!XmlSettingsManager::Instance ().property ("ShowStatusChangesEvents").toBool ())
			return;

		if ((msg->GetMessageSubType () == IMessage::MSTParticipantJoin ||
					msg->GetMessageSubType () == IMessage::MSTParticipantLeave) &&
				!XmlSettingsManager::Instance ().property ("ShowJoinsLeaves").toBool ())
			return;

		if (msg->GetMessageSubType () == IMessage::MSTParticipantEndedConversation)
		{
			if (!XmlSettingsManager::Instance ().property ("ShowEndConversations").toBool ())
				return;
			else if (other)
				msg->SetBody (tr ("%1 ended the conversation.")
						.arg (other->GetEntryName ()));
			else
				msg->SetBody (tr ("Conversation ended."));
		}

		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		emit hookGonnaAppendMsg (proxy, msg->GetQObject ());
		if (proxy->IsCancelled ())
			return;

		if (XmlSettingsManager::Instance ().property ("SeparateMUCEventLogWindow").toBool () &&
			(!parent || parent->GetEntryType () == ICLEntry::ETMUC) &&
			msg->GetMessageType () != IMessage::MTMUCMessage)
		{
			const auto& dt = msg->GetDateTime ().toString ("HH:mm:ss.zzz");
			MUCEventLog_->append (QString ("<font color=\"#56ED56\">[%1] %2</font>")
						.arg (dt)
						.arg (msg->GetBody ()));
			if (msg->GetMessageSubType () != IMessage::MSTRoomSubjectChange)
				return;
		}

		QWebFrame *frame = Ui_.View_->page ()->mainFrame ();

		ChatMsgAppendInfo info =
		{
			Core::Instance ().IsHighlightMessage (msg),
			Core::Instance ().GetChatTabsManager ()->IsActiveChat (GetEntry<ICLEntry> ()),
			ToggleRichText_->isChecked ()
		};

		const auto& links = ProxyObject ().FindLinks (msg->GetBody ());
		if (!links.isEmpty ())
			LastLink_ = links.last ();

		if (!Core::Instance ().AppendMessageByTemplate (frame,
				msg->GetQObject (), info))
			qWarning () << Q_FUNC_INFO
					<< "unhandled append message :(";
	}

	namespace
	{
		void PerformRoleAction (const QPair<QByteArray, QByteArray>& role,
				QObject *mucEntryObj, QString str)
		{
			if (role.first.isEmpty () && role.second.isEmpty ())
				return;

			str = str.trimmed ();
			const int pos = str.lastIndexOf ('|');
			const auto& nick = pos > 0 ? str.left (pos) : str;
			const auto& reason = pos > 0 ? str.mid (pos + 1) : QString ();

			auto mucEntry = qobject_cast<IMUCEntry*> (mucEntryObj);
			auto mucPerms = qobject_cast<IMUCPerms*> (mucEntryObj);

			const auto& parts = mucEntry->GetParticipants ();
			auto partPos = std::find_if (parts.begin (), parts.end (),
					[&nick] (QObject *entryObj) -> bool
					{
						auto entry = qobject_cast<ICLEntry*> (entryObj);
						return entry && entry->GetEntryName () == nick;
					});
			if (partPos == parts.end ())
				return;

			mucPerms->SetPerm (*partPos, role.first, role.second, reason);
		}
	}

	bool ChatTab::ProcessOutgoingMsg (ICLEntry *entry, QString& text)
	{
		IMUCEntry *mucEntry = qobject_cast<IMUCEntry*> (entry->GetQObject ());
		if (entry->GetEntryType () != ICLEntry::ETMUC ||
				!mucEntry)
			return false;

		IMUCPerms *mucPerms = qobject_cast<IMUCPerms*> (entry->GetQObject ());

		if (text.startsWith ("/nick "))
		{
			mucEntry->SetNick (text.mid (std::strlen ("/nick ")));
			return true;
		}
		else if (text.startsWith ("/leave"))
		{
			const int idx = text.indexOf (' ');
			const QString& reason = idx > 0 ?
					text.mid (idx + 1)
					: QString ();
			mucEntry->Leave (reason);
			return true;
		}
		else if (text.startsWith ("/kick ") && mucPerms)
		{
			PerformRoleAction (mucPerms->GetKickPerm (), entry->GetQObject (), text.mid (6));
			return true;
		}
		else if (text.startsWith ("/ban ") && mucPerms)
		{
			PerformRoleAction (mucPerms->GetBanPerm (), entry->GetQObject (), text.mid (5));
			return true;
		}
		else if (text == "/names")
		{
			QStringList names;
			Q_FOREACH (QObject *obj, mucEntry->GetParticipants ())
			{
				ICLEntry *entry = qobject_cast<ICLEntry*> (obj);
				if (!entry)
				{
					qWarning () << Q_FUNC_INFO
							<< obj
							<< "doesn't implement ICLEntry";
					continue;
				}
				const QString& name = entry->GetEntryName ();
				if (!name.isEmpty ())
					names << name;
			}
			names.sort ();
			QWebElement body = Ui_.View_->page ()->mainFrame ()->findFirstElement ("body");
			body.appendInside ("<div class='systemmsg'>" +
					tr ("MUC's participants: ") + "<ul><li>" +
					names.join ("</li><li>") + "</li></ul></div>");
			return true;
		}

		return false;
	}

	void ChatTab::nickComplete ()
	{
		IMUCEntry *entry = GetEntry<IMUCEntry> ();
		if (!entry)
		{
			qWarning () << Q_FUNC_INFO
					<< entry
					<< "doesn't implement ICLEntry";

			return;
		}

		QStringList currentMUCParticipants = GetMUCParticipants ();
		currentMUCParticipants.removeAll (entry->GetNick ());
		if (currentMUCParticipants.isEmpty ())
			return;

		QTextCursor cursor = Ui_.MsgEdit_->textCursor ();

		int cursorPosition = cursor.position ();
		int pos = -1;

		QString text = Ui_.MsgEdit_->toPlainText ();

		if (AvailableNickList_.isEmpty ())
		{
			if (cursorPosition)
				pos = text.lastIndexOf (' ', cursorPosition - 1);
			else
				pos = text.lastIndexOf (' ', cursorPosition);
			LastSpacePosition_ = pos;
		}
		else
			pos = LastSpacePosition_;

		if (NickFirstPart_.isNull ())
		{
			if (!cursorPosition)
				NickFirstPart_ = "";
			else
				NickFirstPart_ = text.mid (pos + 1, cursorPosition - pos -1);
		}

		const QString& post = XmlSettingsManager::Instance ()
				.property ("PostAddressText").toString ();

		if (AvailableNickList_.isEmpty ())
		{
			Q_FOREACH (const QString& item, currentMUCParticipants)
				if (item.startsWith (NickFirstPart_, Qt::CaseInsensitive))
					AvailableNickList_ << item + (pos == -1 ? post : "") + " ";

			if (AvailableNickList_.isEmpty ())
				return;

			text.replace (pos + 1,
					NickFirstPart_.length (),
					AvailableNickList_ [CurrentNickIndex_]);
		}
		else
		{
			QStringList newAvailableNick;

			Q_FOREACH (const QString& item, currentMUCParticipants)
				if (item.startsWith (NickFirstPart_, Qt::CaseInsensitive))
					newAvailableNick << item + (pos == -1 ? post : "") + " ";

			int lastNickLen = -1;
			if ((newAvailableNick != AvailableNickList_) && (!newAvailableNick.isEmpty ()))
			{
				int newIndex = newAvailableNick.indexOf (AvailableNickList_ [CurrentNickIndex_ - 1]);
				lastNickLen = AvailableNickList_ [CurrentNickIndex_ - 1].length ();

				while (newIndex == -1 && CurrentNickIndex_ > 0)
					newIndex = newAvailableNick.indexOf (AvailableNickList_ [--CurrentNickIndex_]);

				CurrentNickIndex_ = (newIndex == -1 ? 0 : newIndex);
				AvailableNickList_ = newAvailableNick;
			}

			if (CurrentNickIndex_ < AvailableNickList_.count () && CurrentNickIndex_)
				text.replace (pos + 1,
						AvailableNickList_ [CurrentNickIndex_ - 1].length (),
						AvailableNickList_ [CurrentNickIndex_]);
			else if (CurrentNickIndex_)
			{
				CurrentNickIndex_ = 0;
				text.replace (pos + 1,
						AvailableNickList_.last ().length (),
						AvailableNickList_ [CurrentNickIndex_]);
			}
			else
				text.replace (pos + 1,
						lastNickLen,
						AvailableNickList_ [CurrentNickIndex_]);
		}
		++CurrentNickIndex_;

		Ui_.MsgEdit_->setPlainText (text);
		cursor.setPosition (pos + 1 + AvailableNickList_ [CurrentNickIndex_ - 1].length ());
		Ui_.MsgEdit_->setTextCursor (cursor);
	}

	QStringList ChatTab::GetMUCParticipants () const
	{
		IMUCEntry *entry = GetEntry<IMUCEntry> ();
		if (!entry)
		{
			qWarning () << Q_FUNC_INFO
					<< entry
					<< "doesn't implement IMUCEntry";

			return QStringList ();
		}

		QStringList participantsList;

		Q_FOREACH (QObject *item, entry->GetParticipants ())
		{
			ICLEntry *part = qobject_cast<ICLEntry*> (item);
			if (!part)
			{
				qWarning () << Q_FUNC_INFO
						<< "unable to cast item to ICLEntry"
						<< item;
				return QStringList ();
			}
			participantsList << part->GetEntryName ();
		}
		return participantsList;
	}

	void ChatTab::ReformatTitle ()
	{
		if (!GetEntry<ICLEntry> ())
		{
			qWarning () << Q_FUNC_INFO
					<< "GetEntry<ICLEntry> returned NULL";
			return;
		}

		QString title = GetEntry<ICLEntry> ()->GetEntryName ();
		if (NumUnreadMsgs_)
			title.prepend (QString ("(%1) ")
					.arg (NumUnreadMsgs_));
		if (HadHighlight_)
			title.prepend ("* ");
		emit changeTabName (this, title);

		QStringList path ("Azoth");
		switch (GetEntry<ICLEntry> ()->GetEntryType ())
		{
		case ICLEntry::ETChat:
			path << tr ("Chat");
			break;
		case ICLEntry::ETMUC:
			path << tr ("Conference");
			break;
		case ICLEntry::ETPrivateChat:
			path << tr ("Private chat");
			break;
		case ICLEntry::ETUnauthEntry:
			path << tr ("Unauthorized user");
			break;
		}

		path << title;

		setProperty ("WidgetLogicalPath", path);
	}

	void ChatTab::UpdateTextHeight ()
	{
		Ui_.CharCounter_->setText (QString::number (Ui_.MsgEdit_->toPlainText ().size ()));

		const int docHeight = Ui_.MsgEdit_->document ()->size ().toSize ().height ();
		if (docHeight == PreviousTextHeight_)
			return;

		PreviousTextHeight_ = docHeight;
		const int numLines = XmlSettingsManager::Instance ().property ("MinLinesHeight").toInt ();
		const int fontHeight = Ui_.MsgEdit_->fontMetrics ().lineSpacing () * numLines +
				Ui_.MsgEdit_->document ()->documentMargin () * 2;
		const int resHeight = std::min (height () / 3, std::max (docHeight, fontHeight));
		Ui_.MsgEdit_->setMinimumHeight (resHeight);
		Ui_.MsgEdit_->setMaximumHeight (resHeight);
	}

	void ChatTab::SetChatPartState (ChatPartState state)
	{
		if (state == PreviousState_)
			return;

		if (IsMUC_)
			return;

		if (!XmlSettingsManager::Instance ()
				.property ("SendChatStates").toBool ())
			return;

		ICLEntry *entry = GetEntry<ICLEntry> ();
		if (!entry)
			return;

		PreviousState_ = state;

		if (state != CPSGone ||
				XmlSettingsManager::Instance ().property ("SendEndConversations").toBool ())
			entry->SetChatPartState (state, Ui_.VariantBox_->currentText ());
	}

	void ChatTab::prepareMessageText (const QString& text)
	{
		Ui_.MsgEdit_->setText (text);
		Ui_.MsgEdit_->moveCursor (QTextCursor::End);
	}

	void ChatTab::appendMessageText (const QString& text)
	{
		prepareMessageText (Ui_.MsgEdit_->toPlainText () + text);
	}

	void ChatTab::selectVariant (const QString& var)
	{
		const int idx = Ui_.VariantBox_->findText (var);
		if (idx == -1)
			return;

		Ui_.VariantBox_->setCurrentIndex (idx);
	}

	QTextEdit* ChatTab::getMsgEdit ()
	{
		return Ui_.MsgEdit_;
	}

	void ChatTab::on_MUCEventsButton__toggled (bool on)
	{
		MUCEventLog_->setVisible (on);
		if (!on)
			return;

		MUCEventLog_->move (Util::FitRectScreen (QCursor::pos () + QPoint (2, 2),
					MUCEventLog_->size ()));
	}

	void ChatTab::handleSeparateMUCLog ()
	{
		MUCEventLog_->clear ();
		const bool isSep = XmlSettingsManager::Instance ()
				.property ("SeparateMUCEventLogWindow").toBool ();

		Ui_.MUCEventsButton_->setVisible (isSep);
		PrepareTheme ();
	}

	void ChatTab::clearAvailableNick ()
	{
		NickFirstPart_.clear ();
		if (!AvailableNickList_.isEmpty ())
		{
			AvailableNickList_.clear ();
			CurrentNickIndex_ = 0;
		}
	}

	void ChatTab::handleEditScroll (int direction)
	{
		int distance = Ui_.View_->size ().height () / 2 - 5;
		Ui_.View_->page ()->mainFrame ()->scroll (0, distance * direction);
	}

	void ChatTab::UpdateStateIcon ()
	{
		emit changeTabIcon (this, TabIcon_);
	}
}
}
