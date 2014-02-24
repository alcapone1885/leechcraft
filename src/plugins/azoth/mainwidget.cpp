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

#include "mainwidget.h"
#include <QMenu>
#include <QMainWindow>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QToolButton>
#include <QInputDialog>
#include <QToolBar>
#include <QShortcut>
#include <QTimer>
#include <util/util.h>
#include <util/gui/clearlineeditaddon.h>
#include <util/shortcuts/shortcutmanager.h>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/iiconthememanager.h>
#include "interfaces/azoth/iclentry.h"
#include "core.h"
#include "sortfilterproxymodel.h"
#include "setstatusdialog.h"
#include "contactlistdelegate.h"
#include "xmlsettingsmanager.h"
#include "addcontactdialog.h"
#include "chattabsmanager.h"
#include "util.h"
#include "groupsenddialog.h"
#include "actionsmanager.h"
#include "accountactionsmanager.h"
#include "bookmarksmanagerdialog.h"
#include "keyboardrosterfixer.h"
#include "statuschangemenumanager.h"
#include "userslistwidget.h"
#include "groupremovedialog.h"

namespace LeechCraft
{
namespace Azoth
{
	MainWidget::MainWidget (AccountActionsManager *accActsMgr, QWidget *parent)
	: QWidget (parent)
	, AccountActsMgr_ (accActsMgr)
	, MainMenu_ (new QMenu (tr ("Azoth menu"), this))
	, MenuButton_ (new QToolButton (this))
	, ProxyModel_ (new SortFilterProxyModel (this))
	, FastStatusButton_ (new QToolButton (this))
	, ActionCLMode_ (new QAction (tr ("CL mode"), this))
	, ActionShowOffline_ (0)
	, BottomBar_ (new QToolBar (tr ("Azoth bar"), this))
	, StatusMenuMgr_ (new StatusChangeMenuManager (this))
	{
		qRegisterMetaType<QPersistentModelIndex> ("QPersistentModelIndex");

		MainMenu_->setIcon (QIcon ("lcicons:/plugins/azoth/resources/images/azoth.svg"));

		BottomBar_->addWidget (MenuButton_);
		BottomBar_->addWidget (FastStatusButton_);

		Ui_.setupUi (this);
		new Util::ClearLineEditAddon (Core::Instance ().GetProxy (), Ui_.FilterLine_);
		Ui_.FilterLine_->setPlaceholderText (tr ("Search..."));
		Ui_.CLTree_->setFocusProxy (Ui_.FilterLine_);

		new KeyboardRosterFixer (Ui_.FilterLine_, Ui_.CLTree_, this);

		Ui_.CLTree_->setItemDelegate (new ContactListDelegate (Ui_.CLTree_));
		ProxyModel_->setSourceModel (Core::Instance ().GetCLModel ());
		Ui_.CLTree_->setModel (ProxyModel_);

		Ui_.CLTree_->viewport ()->setAcceptDrops (true);

		connect (Core::Instance ().GetChatTabsManager (),
				SIGNAL (entryMadeCurrent (QObject*)),
				this,
				SLOT (handleEntryMadeCurrent (QObject*)));
		connect (Core::Instance ().GetChatTabsManager (),
				SIGNAL (entryLostCurrent (QObject*)),
				this,
				SLOT (handleEntryLostCurrent (QObject*)));

		XmlSettingsManager::Instance ().RegisterObject ("EntryActivationType",
				this, "handleEntryActivationType");
		handleEntryActivationType ();

		connect (Ui_.FilterLine_,
				SIGNAL (textChanged (const QString&)),
				ProxyModel_,
				SLOT (setFilterFixedString (const QString&)));

		connect (ProxyModel_,
				SIGNAL (rowsInserted (const QModelIndex&, int, int)),
				this,
				SLOT (handleRowsInserted (const QModelIndex&, int, int)));
		connect (ProxyModel_,
				SIGNAL (rowsRemoved (const QModelIndex&, int, int)),
				this,
				SLOT (rebuildTreeExpansions ()));
		connect (ProxyModel_,
				SIGNAL (modelReset ()),
				this,
				SLOT (rebuildTreeExpansions ()));
		connect (ProxyModel_,
				SIGNAL (mucMode ()),
				Ui_.CLTree_,
				SLOT (expandAll ()));
		connect (ProxyModel_,
				SIGNAL (wholeMode ()),
				this,
				SLOT (resetToWholeMode ()));

		QMetaObject::invokeMethod (Ui_.CLTree_,
				"expandToDepth",
				Qt::QueuedConnection,
				Q_ARG (int, 0));

		if (ProxyModel_->rowCount ())
			QMetaObject::invokeMethod (this,
					"handleRowsInserted",
					Qt::QueuedConnection,
					Q_ARG (QModelIndex, QModelIndex ()),
					Q_ARG (int, 0),
					Q_ARG (int, ProxyModel_->rowCount () - 1));

		CreateMenu ();
		MenuButton_->setMenu (MainMenu_);
		MenuButton_->setIcon (MainMenu_->icon ());
		MenuButton_->setPopupMode (QToolButton::InstantPopup);

		TrayChangeStatus_ = StatusMenuMgr_->CreateMenu (this, SLOT (handleChangeStatusRequested ()), this);

		FastStatusButton_->setMenu (StatusMenuMgr_->CreateMenu (this, SLOT (fastStateChangeRequested ()), this));
		FastStatusButton_->setPopupMode (QToolButton::InstantPopup);
		updateFastStatusButton (SOffline);

		ActionDeleteSelected_ = new QAction (this);
		ActionDeleteSelected_->setShortcut (Qt::Key_Delete);
		ActionDeleteSelected_->setShortcutContext (Qt::WidgetWithChildrenShortcut);
		connect (ActionDeleteSelected_,
				SIGNAL (triggered ()),
				this,
				SLOT (handleDeleteSelected ()));
		addAction (ActionDeleteSelected_);

		XmlSettingsManager::Instance ().RegisterObject ("ShowMenuBar",
				this, "menuBarVisibilityToggled");
		menuBarVisibilityToggled ();

		XmlSettingsManager::Instance ().RegisterObject ("StatusIcons",
				this, "handleStatusIconsChanged");
		handleStatusIconsChanged ();

		connect (&Core::Instance (),
				SIGNAL (topStatusChanged (LeechCraft::Azoth::State)),
				this,
				SLOT (updateFastStatusButton (LeechCraft::Azoth::State)));

		qobject_cast<QVBoxLayout*> (layout ())->insertWidget (0, BottomBar_);

		auto sm = Core::Instance ().GetShortcutManager ();
		auto listShortcut = new QShortcut (QString ("Alt+C"),
				this, SLOT (showAllUsersList ()));
		listShortcut->setContext (Qt::ApplicationShortcut);
		sm->RegisterShortcut ("org.LeechCraft.Azoth.AllUsersList",
				{
					tr ("Show all users list"),
					{ "Alt+C" },
					Core::Instance ().GetProxy ()->GetIconThemeManager ()->GetIcon ("system-users")
				},
				listShortcut);
	}

	QList<QAction*> MainWidget::GetMenuActions ()
	{
		return MainMenu_->actions ();
	}

	QMenu* MainWidget::GetChangeStatusMenu () const
	{
		return TrayChangeStatus_;
	}

	void MainWidget::CreateMenu ()
	{
		MainMenu_->addSeparator ();

		QAction *addContact = MainMenu_->addAction (tr ("Add contact..."),
				this,
				SLOT (handleAddContactRequested ()));
		addContact->setProperty ("ActionIcon", "list-add-user");

		QAction *joinConf = MainMenu_->addAction (tr ("Join conference..."),
				&Core::Instance (),
				SLOT (handleMucJoinRequested ()));
		joinConf->setProperty ("ActionIcon", "irc-join-channel");

		MainMenu_->addSeparator ();
		MainMenu_->addAction (tr ("Manage bookmarks..."),
				this,
				SLOT (handleManageBookmarks ()));
		MainMenu_->addSeparator ();
		MainMenu_->addAction (tr ("Add account..."),
				this,
				SLOT (handleAddAccountRequested ()));
		MainMenu_->addSeparator ();

		ActionShowOffline_ = MainMenu_->addAction (tr ("Show offline contacts"));
		ActionShowOffline_->setCheckable (true);
		bool show = XmlSettingsManager::Instance ()
				.Property ("ShowOfflineContacts", true).toBool ();
		ProxyModel_->showOfflineContacts (show);
		ActionShowOffline_->setChecked (show);
		connect (ActionShowOffline_,
				SIGNAL (toggled (bool)),
				this,
				SLOT (handleShowOffline (bool)));

		ActionCLMode_->setCheckable (true);
		ActionCLMode_->setProperty ("ActionIcon", "meeting-attending");
		ActionCLMode_->setShortcut (QString ("Ctrl+Shift+R"));
		Core::Instance ().GetShortcutManager ()->RegisterAction ("org.LeechCraft.Azoth.CLMode", ActionCLMode_);
		connect (ActionCLMode_,
				SIGNAL (toggled (bool)),
				this,
				SLOT (handleCLMode (bool)));

		BottomBar_->setToolButtonStyle (Qt::ToolButtonIconOnly);

		auto addBottomAct = [this] (QAction *act)
		{
			const QString& icon = act->property ("ActionIcon").toString ();
			act->setIcon (Core::Instance ().GetProxy ()->GetIconThemeManager ()->GetIcon (icon));
			BottomBar_->addAction (act);
		};
		addBottomAct (addContact);
		addBottomAct (ActionShowOffline_);
		addBottomAct (ActionCLMode_);
	}

	void MainWidget::handleAccountVisibilityChanged ()
	{
		ProxyModel_->invalidate ();
	}

	void MainWidget::updateFastStatusButton (State state)
	{
		FastStatusButton_->setIcon (Core::Instance ().GetIconForState (state));
	}

	void MainWidget::treeActivated (const QModelIndex& index)
	{
		if (index.data (Core::CLREntryType).value<Core::CLEntryType> () != Core::CLETContact)
			return;

		if (QApplication::keyboardModifiers () & Qt::CTRL)
			if (auto tab = Core::Instance ().GetChatTabsManager ()->GetActiveChatTab ())
			{
				auto text = index.data ().toString ();

				auto edit = tab->getMsgEdit ();
				if (edit->toPlainText ().isEmpty ())
					text += XmlSettingsManager::Instance ()
							.property ("PostAddressText").toString ();
				else
					text.prepend (" ");

				tab->appendMessageText (text);
				return;
			}

		Core::Instance ().OpenChat (ProxyModel_->mapToSource (index));
	}

	void MainWidget::showAllUsersList ()
	{
		QList<QObject*> entries;
		int accCount = 0;
		for (auto acc : Core::Instance ().GetAccounts ())
		{
			if (!acc->IsShownInRoster ())
				continue;

			++accCount;
			const auto& accEntries = acc->GetCLEntries ();
			std::copy_if (accEntries.begin (), accEntries.end (), std::back_inserter (entries),
					[] (QObject *entryObj) -> bool
					{
						auto entry = qobject_cast<ICLEntry*> (entryObj);
						return entry->GetEntryType () != ICLEntry::ETPrivateChat;
					});
		}

		UsersListWidget w (entries,
				[accCount] (ICLEntry *entry) -> QString
				{
					QString text = entry->GetEntryName ();
					if (text != entry->GetHumanReadableID ())
						text += " (" + entry->GetHumanReadableID () + ")";

					if (accCount <= 1)
						return text;

					auto acc = qobject_cast<IAccount*> (entry->GetParentAccount ());
					return text + " [" + acc->GetAccountName () + "]";
				},
				this);
		if (w.exec () != QDialog::Accepted)
			return;

		if (auto entry = w.GetActivatedParticipant ())
			Core::Instance ().GetChatTabsManager ()->
					OpenChat (qobject_cast<ICLEntry*> (entry), true);
	}

	void MainWidget::on_CLTree__customContextMenuRequested (const QPoint& pos)
	{
		const QModelIndex& index = ProxyModel_->mapToSource (Ui_.CLTree_->indexAt (pos));
		if (!index.isValid ())
			return;

		ActionsManager *manager = Core::Instance ().GetActionsManager ();

		QMenu *menu = new QMenu (tr ("Entry context menu"));
		QList<QAction*> actions;
		switch (index.data (Core::CLREntryType).value<Core::CLEntryType> ())
		{
		case Core::CLETContact:
		{
			auto rows = Ui_.CLTree_->selectionModel ()->selectedRows ();
			for (auto& row : rows)
				row = ProxyModel_->mapToSource (row);

			if (!rows.contains (index))
				rows << index;

			if (rows.size () == 1)
			{
				QObject *obj = index.data (Core::CLREntryObject).value<QObject*> ();
				ICLEntry *entry = qobject_cast<ICLEntry*> (obj);

				const auto& allActions = manager->GetEntryActions (entry);
				for (auto action : allActions)
				{
					const auto& areas = manager->GetAreasForAction (action);
					if (action->isSeparator () ||
							areas.contains (ActionsManager::CLEAAContactListCtxtMenu))
						actions << action;
				}
			}
			else
			{
				QList<ICLEntry*> entries;
				for (const auto& row : rows)
				{
					const auto entryObj = row.data (Core::CLREntryObject).value<QObject*> ();
					const auto entry = qobject_cast<ICLEntry*> (entryObj);
					if (entry)
						entries << entry;
				}

				const auto& allActions = manager->CreateEntriesActions (entries, menu);
				for (auto action : allActions)
				{
					const auto& areas = manager->GetAreasForAction (action);
					if (action->isSeparator () ||
							areas.contains (ActionsManager::CLEAAContactListCtxtMenu))
						actions << action;
				}
			}
			break;
		}
		case Core::CLETCategory:
		{
			QAction *rename = new QAction (tr ("Rename group..."), this);
			QVariant objVar = index.parent ().data (Core::CLRAccountObject);;
			rename->setProperty ("Azoth/OldGroupName", index.data ());
			rename->setProperty ("Azoth/AccountObject", objVar);
			connect (rename,
					SIGNAL (triggered ()),
					this,
					SLOT (handleCatRenameTriggered ()));
			actions << rename;

			QList<QVariant> entries;
			for (int i = 0, cnt = index.model ()->rowCount (index); i < cnt; ++i)
				entries << index.child (i, 0).data (Core::CLREntryObject);

			QAction *sendMsg = new QAction (tr ("Send message..."), menu);
			sendMsg->setProperty ("Azoth/Entries", entries);
			connect (sendMsg,
					SIGNAL (triggered ()),
					this,
					SLOT (handleSendGroupMsgTriggered ()));
			actions << sendMsg;

			if (index.data (Core::CLRUnreadMsgCount).toInt ())
			{
				auto markAll = new QAction (tr ("Mark all messages as read"), menu);
				markAll->setProperty ("Azoth/Entries", entries);
				connect (markAll,
						SIGNAL (triggered ()),
						this,
						SLOT (handleMarkAllTriggered ()));
				actions << markAll;
			}

			QAction *removeChildren = new QAction (tr ("Remove group's participants"), menu);
			removeChildren->setProperty ("Azoth/Entries", entries);
			connect (removeChildren,
					SIGNAL (triggered ()),
					this,
					SLOT (handleRemoveChildrenTriggered ()));
			actions << removeChildren;
			break;
		}
		case Core::CLETAccount:
			actions << AccountActsMgr_->GetMenuActions (menu,
					index.data (Core::CLRAccountObject).value<QObject*> ());
			break;
		default:
			break;
		}
		if (!actions.size ())
		{
			delete menu;
			return;
		}

		menu->addActions (actions);
		menu->exec (Ui_.CLTree_->mapToGlobal (pos));
		menu->deleteLater ();
	}

	void MainWidget::handleChangeStatusRequested ()
	{
		QAction *action = qobject_cast<QAction*> (sender ());
		if (!action)
		{
			qWarning () << Q_FUNC_INFO
					<< sender ()
					<< "is not an action";
			return;
		}

		QVariant stateVar = action->property ("Azoth/TargetState");
		EntryStatus status;
		if (!stateVar.isNull ())
		{
			const auto state = stateVar.value<State> ();
			status = EntryStatus (state, AccountActsMgr_->GetStatusText (action, state));
		}
		else
		{
			SetStatusDialog ssd ("global", this);
			if (ssd.exec () != QDialog::Accepted)
				return;

			status = EntryStatus (ssd.GetState (), ssd.GetStatusText ());
		}

		for (IAccount *acc : Core::Instance ().GetAccounts ())
			if (acc->IsShownInRoster ())
				acc->ChangeState (status);
		updateFastStatusButton (status.State_);
	}

	void MainWidget::fastStateChangeRequested ()
	{
		const auto& stateVar = sender ()->property ("Azoth/TargetState");
		if (stateVar.isNull ())
		{
			handleChangeStatusRequested ();
			return;
		}

		const auto state = stateVar.value<State> ();
		updateFastStatusButton (state);

		const EntryStatus status (state,
				AccountActsMgr_->GetStatusText (static_cast<QAction*> (sender ()), state));
		for (IAccount *acc : Core::Instance ().GetAccounts ())
			if (acc->IsShownInRoster ())
				acc->ChangeState (status);
	}

	void MainWidget::handleEntryActivationType ()
	{
		disconnect (Ui_.CLTree_,
				0,
				this,
				SLOT (treeActivated (const QModelIndex&)));
		disconnect (Ui_.CLTree_,
				0,
				this,
				SLOT (clearFilter ()));

		const char *signal = 0;
		const QString& actType = XmlSettingsManager::Instance ()
				.property ("EntryActivationType").toString ();

		if (actType == "click")
			signal = SIGNAL (clicked (const QModelIndex&));
		else if (actType == "dclick")
			signal = SIGNAL (doubleClicked (const QModelIndex&));
		else
			signal = SIGNAL (activated (const QModelIndex&));

		connect (Ui_.CLTree_,
				signal,
				this,
				SLOT (treeActivated (const QModelIndex&)));
		connect (Ui_.CLTree_,
				signal,
				this,
				SLOT (clearFilter ()));
	}

	void MainWidget::handleCatRenameTriggered ()
	{
		if (!sender ())
		{
			qWarning () << Q_FUNC_INFO
					<< "null sender()";
			return;
		}
		sender ()->deleteLater ();

		const QString& group = sender ()->property ("Azoth/OldGroupName").toString ();

		const QString& newGroup = QInputDialog::getText (this,
				tr ("Rename group"),
				tr ("Enter new group name for %1:")
					.arg (group),
				QLineEdit::Normal,
				group);
		if (newGroup.isEmpty () || newGroup == group)
			return;

		QObject *accObj = sender ()->property ("Azoth/AccountObject").value<QObject*> ();
		IAccount *acc = qobject_cast<IAccount*> (accObj);
		if (!acc)
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to cast"
					<< accObj
					<< "to IAccount";
			return;
		}

		Q_FOREACH (QObject *entryObj, acc->GetCLEntries ())
		{
			ICLEntry *entry = qobject_cast<ICLEntry*> (entryObj);
			if (!entry)
			{
				qWarning () << Q_FUNC_INFO
						<< "unable to cast"
						<< entryObj
						<< "to ICLEntry";
				continue;
			}

			QStringList groups = entry->Groups ();
			if (groups.removeAll (group))
			{
				groups << newGroup;
				entry->SetGroups (groups);
			}
		}
	}

	namespace
	{
		QList<QObject*> GetEntriesFromSender (QObject *sender)
		{
			QList<QObject*> entries;
			for (const auto& entryVar : sender->property ("Azoth/Entries").toList ())
				entries << entryVar.value<QObject*> ();
			return entries;
		}
	}

	void MainWidget::handleSendGroupMsgTriggered ()
	{
		const auto& entries = GetEntriesFromSender (sender ());

		auto dlg = new GroupSendDialog (entries, this);
		dlg->setAttribute (Qt::WA_DeleteOnClose, true);
		dlg->show ();
	}

	void MainWidget::handleMarkAllTriggered ()
	{
		for (const auto entry : GetEntriesFromSender (sender ()))
			qobject_cast<ICLEntry*> (entry)->MarkMsgsRead ();
	}

	void MainWidget::handleRemoveChildrenTriggered ()
	{
		auto entries = GetEntriesFromSender (sender ());
		for (auto i = entries.begin (); i != entries.end (); )
		{
			auto entry = qobject_cast<ICLEntry*> (*i);
			if (!entry ||
				(entry->GetEntryFeatures () & ICLEntry::FMaskLongetivity) != ICLEntry::FPermanentEntry)
				i = entries.erase (i);
			else
				++i;
		}

		if (entries.isEmpty ())
			return;

		auto dlg = new GroupRemoveDialog (entries, this);
		dlg->setAttribute (Qt::WA_DeleteOnClose, true);
		dlg->show ();
	}

	void MainWidget::handleManageBookmarks ()
	{
		BookmarksManagerDialog *dia = new BookmarksManagerDialog (this);
		dia->show ();
	}

	void MainWidget::handleAddAccountRequested ()
	{
		InitiateAccountAddition (this);
	}

	void MainWidget::handleAddContactRequested ()
	{
		std::unique_ptr<AddContactDialog> dia (new AddContactDialog (0, this));
		if (dia->exec () != QDialog::Accepted)
			return;

		if (!dia->GetSelectedAccount ())
			return;

		dia->GetSelectedAccount ()->RequestAuth (dia->GetContactID (),
					dia->GetReason (),
					dia->GetNick (),
					dia->GetGroups ());
	}

	void MainWidget::handleShowOffline (bool show)
	{
		XmlSettingsManager::Instance ().setProperty ("ShowOfflineContacts", show);
		ProxyModel_->showOfflineContacts (show);
	}

	void MainWidget::clearFilter ()
	{
		if (!XmlSettingsManager::Instance ().property ("ClearSearchAfterFocus").toBool ())
			return;

		if (!Ui_.FilterLine_->text ().isEmpty ())
			Ui_.FilterLine_->setText (QString ());
	}

	void MainWidget::handleDeleteSelected ()
	{
		const auto& idx = ProxyModel_->mapToSource (Ui_.CLTree_->currentIndex ());
		if (!idx.isValid ())
			return;

		if (idx.data (Core::CLREntryType).value<Core::CLEntryType> () != Core::CLETContact)
			return;

		const auto& obj = idx.data (Core::CLREntryObject).value<QObject*> ();
		auto entry = qobject_cast<ICLEntry*> (obj);
		auto acc = entry ? qobject_cast<IAccount*> (entry->GetParentAccount ()) : 0;
		if (!entry || !acc)
		{
			qWarning () << Q_FUNC_INFO
					<< "no entry or account";
			return;
		}

		if (QMessageBox::question (this,
				"LeechCraft",
				tr ("Are you sure you want to remove %1 from roster?")
					.arg (entry->GetEntryName ()),
				QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;

		acc->RemoveEntry (obj);
	}

	void MainWidget::handleEntryMadeCurrent (QObject *obj)
	{
		auto entry = qobject_cast<ICLEntry*> (obj);
		if (entry && entry->GetEntryType () == ICLEntry::ETPrivateChat)
			obj = entry->GetParentCLEntry ();

		const bool isMUC = qobject_cast<IMUCEntry*> (obj);

		if (XmlSettingsManager::Instance ().property ("AutoMUCMode").toBool ())
			ActionCLMode_->setChecked (isMUC);

		if (isMUC)
			ProxyModel_->SetMUC (obj);
	}

	void MainWidget::handleEntryLostCurrent (QObject*)
	{
		if (XmlSettingsManager::Instance ().property ("AutoMUCMode").toBool ())
			ActionCLMode_->setChecked (false);
	}

	void MainWidget::resetToWholeMode ()
	{
		ActionCLMode_->setChecked (false);
	}

	void MainWidget::handleCLMode (bool mucMode)
	{
		if (mucMode)
		{
			FstLevelExpands_.clear ();
			SndLevelExpands_.clear ();

			for (int i = 0; i < ProxyModel_->rowCount (); ++i)
			{
				const QModelIndex& accIdx = ProxyModel_->index (i, 0);
				const QString& name = accIdx.data ().toString ();
				FstLevelExpands_ [name] = Ui_.CLTree_->isExpanded (accIdx);

				QMap<QString, bool> groups;
				for (int j = 0, rc = ProxyModel_->rowCount (accIdx);
						j < rc; ++j)
				{
					const QModelIndex& grpIdx = ProxyModel_->index (j, 0, accIdx);
					groups [grpIdx.data ().toString ()] = Ui_.CLTree_->isExpanded (grpIdx);
				}

				SndLevelExpands_ [name] = groups;
			}
		}

		ProxyModel_->SetMUCMode (mucMode);

		if (!mucMode &&
				!FstLevelExpands_.isEmpty () &&
				!SndLevelExpands_.isEmpty ())
		{
			for (int i = 0; i < ProxyModel_->rowCount (); ++i)
			{
				const QModelIndex& accIdx = ProxyModel_->index (i, 0);
				const QString& name = accIdx.data ().toString ();
				if (!FstLevelExpands_.contains (name))
					continue;

				Ui_.CLTree_->setExpanded (accIdx, FstLevelExpands_.take (name));

				const QMap<QString, bool>& groups = SndLevelExpands_.take (name);
				for (int j = 0, rc = ProxyModel_->rowCount (accIdx);
						j < rc; ++j)
				{
					const QModelIndex& grpIdx = ProxyModel_->index (j, 0, accIdx);
					Ui_.CLTree_->setExpanded (grpIdx, groups [grpIdx.data ().toString ()]);
				}
			}
		}
	}

	void MainWidget::menuBarVisibilityToggled ()
	{
		BottomBar_->setVisible (XmlSettingsManager::Instance ().property ("ShowMenuBar").toBool ());
	}

	void MainWidget::handleStatusIconsChanged ()
	{
		ActionShowOffline_->setIcon (Core::Instance ().GetIconForState (SOffline));
	}

	namespace
	{
		QString BuildPath (const QModelIndex& index)
		{
			if (!index.isValid ())
				return QString ();

			QString path = "CLTreeState/Expanded/" + index.data ().toString ();
			QModelIndex parent = index;
			while ((parent = parent.parent ()).isValid ())
				path.prepend (parent.data ().toString () + "/");

			path = path.toUtf8 ().toBase64 ().replace ('/', '_');

			return path;
		}
	}

	void MainWidget::handleRowsInserted (const QModelIndex& parent, int begin, int end)
	{
		for (int i = begin; i <= end; ++i)
		{
			const auto& index = ProxyModel_->index (i, 0, parent);
			if (!index.isValid ())
			{
				qWarning () << Q_FUNC_INFO
						<< "invalid index"
						<< parent
						<< i
						<< "in"
						<< begin
						<< end;
				continue;
			}

			const auto type = index.data (Core::CLREntryType).value<Core::CLEntryType> ();
			if (type == Core::CLETCategory)
			{
				const QString& path = BuildPath (index);

				const bool expanded = ProxyModel_->IsMUCMode () ||
						XmlSettingsManager::Instance ().Property (path, true).toBool ();
				if (expanded)
					QMetaObject::invokeMethod (this,
							"expandIndex",
							Qt::QueuedConnection,
							Q_ARG (QPersistentModelIndex, QPersistentModelIndex (index)));

				if (ProxyModel_->rowCount (index))
					handleRowsInserted (index, 0, ProxyModel_->rowCount (index) - 1);
			}
			else if (type == Core::CLETAccount)
			{
				QMetaObject::invokeMethod (this,
						"expandIndex",
						Qt::QueuedConnection,
						Q_ARG (QPersistentModelIndex, QPersistentModelIndex (index)));

				if (ProxyModel_->rowCount (index))
					handleRowsInserted (index, 0, ProxyModel_->rowCount (index) - 1);
			}
		}
	}

	void MainWidget::rebuildTreeExpansions ()
	{
		if (ProxyModel_->rowCount ())
			handleRowsInserted (QModelIndex (), 0, ProxyModel_->rowCount () - 1);
	}

	void MainWidget::expandIndex (const QPersistentModelIndex& pIdx)
	{
		if (!pIdx.isValid ())
			return;

		Ui_.CLTree_->expand (pIdx);
	}

	namespace
	{
		void SetExpanded (const QModelIndex& idx, bool expanded)
		{
			const QString& path = BuildPath (idx);
			if (path.isEmpty ())
				return;

			XmlSettingsManager::Instance ().setProperty (path.toUtf8 (), expanded);
		}
	}

	void MainWidget::on_CLTree__collapsed (const QModelIndex& idx)
	{
		SetExpanded (idx, false);
	}

	void MainWidget::on_CLTree__expanded (const QModelIndex& idx)
	{
		SetExpanded (idx, true);
	}
}
}
