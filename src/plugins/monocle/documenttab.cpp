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

#include "documenttab.h"
#include <functional>
#include <QToolBar>
#include <QComboBox>
#include <QFileDialog>
#include <QLineEdit>
#include <QMenu>
#include <QToolButton>
#include <QPrinter>
#include <QPrintDialog>
#include <QMessageBox>
#include <QDockWidget>
#include <QClipboard>
#include <QtDebug>
#include <QMimeData>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QImageWriter>
#include <QTimer>
#include <QScrollBar>
#include <QUrl>
#include <util/util.h>
#include <util/xpc/stddatafiltermenucreator.h>
#include <interfaces/imwproxy.h>
#include <interfaces/core/irootwindowsmanager.h>
#include "interfaces/monocle/ihavetoc.h"
#include "interfaces/monocle/ihavetextcontent.h"
#include "interfaces/monocle/isupportannotations.h"
#include "interfaces/monocle/idynamicdocument.h"
#include "core.h"
#include "pagegraphicsitem.h"
#include "filewatcher.h"
#include "tocwidget.h"
#include "presenterwidget.h"
#include "recentlyopenedmanager.h"
#include "common.h"
#include "docstatemanager.h"
#include "docinfodialog.h"
#include "xmlsettingsmanager.h"
#include "bookmarkswidget.h"
#include "pageslayoutmanager.h"
#include "thumbswidget.h"

namespace LeechCraft
{
namespace Monocle
{
	DocumentTab::DocumentTab (const TabClassInfo& tc, QObject *parent)
	: TC_ (tc)
	, ParentPlugin_ (parent)
	, Toolbar_ (new QToolBar ("Monocle"))
	, ScalesBox_ (0)
	, PageNumLabel_ (0)
	, LayOnePage_ (0)
	, LayTwoPages_ (0)
	, LayoutManager_ (0)
	, DockWidget_ (new QDockWidget (tr ("Monocle dock")))
	, TOCWidget_ (new TOCWidget ())
	, BMWidget_ (new BookmarksWidget (this))
	, ThumbsWidget_ (new ThumbsWidget ())
	, MouseMode_ (MouseMode::Move)
	, SaveStateScheduled_ (false)
	, Onload_ ({ -1, 0, 0 })
	{
		Ui_.setupUi (this);
		Ui_.PagesView_->setScene (&Scene_);
		Ui_.PagesView_->setBackgroundBrush (palette ().brush (QPalette::Dark));
		Ui_.PagesView_->SetDocumentTab (this);

		LayoutManager_ = new PagesLayoutManager (Ui_.PagesView_, this);

		SetupToolbar ();

		new FileWatcher (this);

		auto proxy = Core::Instance ().GetProxy ();
		const auto& tocIcon = proxy->GetIcon ("view-table-of-contents-ltr");

		auto dockTabWidget = new QTabWidget;
		dockTabWidget->setTabPosition (QTabWidget::West);
		dockTabWidget->addTab (TOCWidget_,
				tocIcon, tr ("Table of contents"));
		dockTabWidget->addTab (BMWidget_,
				proxy->GetIcon ("favorites"), tr ("Bookmarks"));
		dockTabWidget->addTab (ThumbsWidget_,
				proxy->GetIcon ("view-preview"), tr ("Thumbnails"));

		connect (ThumbsWidget_,
				SIGNAL (pageClicked (int)),
				this,
				SLOT (handleThumbnailClicked (int)));

		DockWidget_->setFeatures (QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
		DockWidget_->setAllowedAreas (Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
		DockWidget_->setWidget (dockTabWidget);

		DockWidget_->setWindowIcon (tocIcon);
		DockWidget_->toggleViewAction ()->setIcon (tocIcon);

		Toolbar_->addSeparator ();
		Toolbar_->addAction (DockWidget_->toggleViewAction ());

		auto dwa = static_cast<Qt::DockWidgetArea> (XmlSettingsManager::Instance ()
				.Property ("DockWidgetArea", Qt::RightDockWidgetArea).toInt ());
		if (dwa == Qt::NoDockWidgetArea)
			dwa = Qt::RightDockWidgetArea;

		auto mw = Core::Instance ().GetProxy ()->GetRootWindowsManager ()->GetMWProxy (0);
		mw->AddDockWidget (dwa, DockWidget_);
		mw->AssociateDockWidget (DockWidget_, this);
		mw->ToggleViewActionVisiblity (DockWidget_, false);
		connect (DockWidget_,
				SIGNAL (dockLocationChanged (Qt::DockWidgetArea)),
				this,
				SLOT (handleDockLocation (Qt::DockWidgetArea)));

		connect (this,
				SIGNAL (currentPageChanged (int)),
				this,
				SLOT (updateNumLabel ()));
		connect (this,
				SIGNAL (currentPageChanged (int)),
				ThumbsWidget_,
				SLOT (handleCurrentPage (int)));
		connect (this,
				SIGNAL (pagesVisibilityChanged (QMap<int, QRect>)),
				ThumbsWidget_,
				SLOT (updatePagesVisibility (QMap<int, QRect>)));
	}

	TabClassInfo DocumentTab::GetTabClassInfo () const
	{
		return TC_;
	}

	QObject* DocumentTab::ParentMultiTabs ()
	{
		return ParentPlugin_;
	}

	void DocumentTab::Remove ()
	{
		DockWidget_->widget ()->deleteLater ();
		DockWidget_->deleteLater ();
		TOCWidget_->deleteLater ();
		BMWidget_->deleteLater ();
		ThumbsWidget_->deleteLater ();

		emit removeTab (this);
		deleteLater ();
	}

	QToolBar* DocumentTab::GetToolBar () const
	{
		return Toolbar_;
	}

	QString DocumentTab::GetTabRecoverName () const
	{
		return CurrentDocPath_.isEmpty () ?
				QString () :
				"Monocle: " + QFileInfo (CurrentDocPath_).fileName ();
	}

	QIcon DocumentTab::GetTabRecoverIcon () const
	{
		return TC_.Icon_;
	}

	QByteArray DocumentTab::GetTabRecoverData () const
	{
		if (CurrentDocPath_.isEmpty ())
			return QByteArray ();

		QByteArray result;
		QDataStream out (&result, QIODevice::WriteOnly);
		out << static_cast<quint8> (1)
			<< CurrentDocPath_
			<< LayoutManager_->GetCurrentScale ()
			<< Ui_.PagesView_->mapToScene (GetViewportCenter ()).toPoint ();

		switch (LayoutManager_->GetLayoutMode ())
		{
		case LayoutMode::OnePage:
			out << QByteArray ("one");
			break;
		case LayoutMode::TwoPages:
			out << QByteArray ("two");
			break;
		}

		return result;
	}

	void DocumentTab::FillMimeData (QMimeData *data)
	{
		if (CurrentDocPath_.isEmpty ())
			return;

		data->setUrls ({ QUrl::fromLocalFile (CurrentDocPath_) });
		data->setText (QFileInfo (CurrentDocPath_).fileName ());
	}

	void DocumentTab::HandleDragEnter (QDragMoveEvent *event)
	{
		auto data = event->mimeData ();

		if (!data->hasUrls ())
			return;

		const auto& url = data->urls ().first ();
		if (!url.isLocalFile () || !QFile::exists (url.toLocalFile ()))
			return;

		const auto& localPath = url.toLocalFile ();
		if (Core::Instance ().CanLoadDocument (localPath))
			event->acceptProposedAction ();
	}

	void DocumentTab::HandleDrop (QDropEvent *event)
	{
		auto data = event->mimeData ();

		if (!data->hasUrls ())
			return;

		const auto& url = data->urls ().first ();
		if (!url.isLocalFile () || !QFile::exists (url.toLocalFile ()))
			return;

		SetDoc (url.toLocalFile ());
		event->acceptProposedAction ();
	}

	void DocumentTab::RecoverState (const QByteArray& data)
	{
		QDataStream in (data);
		quint8 version = 0;
		in >> version;
		if (version != 1)
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown state version"
					<< version;
			return;
		}

		QString path;
		double scale = 0;
		QPoint point;
		QByteArray modeStr;
		in >> path
			>> scale
			>> point
			>> modeStr;

		if (modeStr == "one")
			LayoutManager_->SetLayoutMode (LayoutMode::OnePage);
		else if (modeStr == "two")
			LayoutManager_->SetLayoutMode (LayoutMode::TwoPages);

		SetDoc (path);
		LayoutManager_->SetScaleMode (ScaleMode::Fixed);
		LayoutManager_->SetFixedScale (scale);
		Relayout ();
		QMetaObject::invokeMethod (this,
				"delayedCenterOn",
				Qt::QueuedConnection,
				Q_ARG (QPoint, point));
	}

	void DocumentTab::ReloadDoc (const QString& doc)
	{
		Scene_.clear ();
		Pages_.clear ();
		CurrentDoc_ = IDocument_ptr ();
		CurrentDocPath_.clear ();

		const auto& pos = Ui_.PagesView_->GetCurrentCenter ();

		SetDoc (doc);

		if (Scene_.itemsBoundingRect ().contains (pos))
			Ui_.PagesView_->centerOn (pos);
	}

	bool DocumentTab::SetDoc (const QString& path)
	{
		if (SaveStateScheduled_)
			saveState ();

		auto document = Core::Instance ().LoadDocument (path);
		if (!document || !document->IsValid ())
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to navigate to"
					<< path;
			QMessageBox::warning (this,
					"LeechCraft",
					tr ("Unable to open document %1.")
						.arg ("<em>" + path + "</em>"));
			return false;
		}

		const auto& state = Core::Instance ()
				.GetDocStateManager ()->GetState (QFileInfo (path).fileName ());

		Core::Instance ().GetROManager ()->RecordOpened (path);

		Scene_.clear ();
		Pages_.clear ();

		CurrentDoc_ = document;
		CurrentDocPath_ = path;
		const auto& title = QFileInfo (path).fileName ();
		emit changeTabName (this, title);

		for (int i = 0, size = CurrentDoc_->GetNumPages (); i < size; ++i)
		{
			auto item = new PageGraphicsItem (CurrentDoc_, i);
			Scene_.addItem (item);
			Pages_ << item;
		}

		LayoutManager_->HandleDoc (CurrentDoc_, Pages_);

		recoverDocState (state);
		Relayout ();
		SetCurrentPage (state.CurrentPage_, false);

		checkCurrentPageChange (true);

		TOCEntryLevel_t topLevel;
		if (auto toc = qobject_cast<IHaveTOC*> (CurrentDoc_->GetQObject ()))
			topLevel = toc->GetTOC ();
		TOCWidget_->SetTOC (topLevel);
		TOCWidget_->setEnabled (!topLevel.isEmpty ());

		connect (CurrentDoc_->GetQObject (),
				SIGNAL (navigateRequested (QString, int, double, double)),
				this,
				SLOT (handleNavigateRequested (QString, int, double, double)),
				Qt::QueuedConnection);

		emit fileLoaded (path);

		emit tabRecoverDataChanged ();

		if (qobject_cast<IDynamicDocument*> (CurrentDoc_->GetQObject ()))
			connect (CurrentDoc_->GetQObject (),
					SIGNAL (pageContentsChanged (int)),
					this,
					SLOT (handlePageContentsChanged (int)));

		BMWidget_->HandleDoc (CurrentDoc_);
		ThumbsWidget_->HandleDoc (CurrentDoc_);

		return true;
	}

	void DocumentTab::CreateViewCtxMenuActions (QMenu *menu)
	{
		auto copyAsImage = menu->addAction (tr ("Copy selection as image"),
				this, SLOT (handleCopyAsImage ()));
		copyAsImage->setProperty ("ActionIcon", "image-x-generic");

		auto saveAsImage = menu->addAction (tr ("Save selection as image..."),
				this, SLOT (handleSaveAsImage ()));
		saveAsImage->setProperty ("ActionIcon", "document-save");

		if (qobject_cast<IHaveTextContent*> (CurrentDoc_->GetQObject ()))
		{
			menu->addSeparator ();

			const auto& selText = GetSelectionText ();

			auto copyAsText = menu->addAction (tr ("Copy selection as text"),
					this, SLOT (handleCopyAsText ()));
			copyAsText->setProperty ("Monocle/Text", selText);
			copyAsText->setProperty ("ActionIcon", "edit-copy");

			new Util::StdDataFilterMenuCreator (selText,
					Core::Instance ().GetProxy ()->GetEntityManager (),
					menu);
		}
	}

	int DocumentTab::GetCurrentPage () const
	{
		return LayoutManager_->GetCurrentPage ();
	}

	void DocumentTab::SetCurrentPage (int idx, bool immediate)
	{
		LayoutManager_->SetCurrentPage (idx, immediate);
	}

	QPoint DocumentTab::GetCurrentCenter () const
	{
		return Ui_.PagesView_->GetCurrentCenter ().toPoint ();
	}

	void DocumentTab::CenterOn (const QPoint& point)
	{
		Ui_.PagesView_->SmoothCenterOn (point.x (), point.y ());
	}

	void DocumentTab::SetupToolbar ()
	{
		auto open = new QAction (tr ("Open..."), this);
		open->setProperty ("ActionIcon", "document-open");
		open->setShortcut (QString ("Ctrl+O"));
		connect (open,
				SIGNAL (triggered ()),
				this,
				SLOT (selectFile ()));

		auto roMenu = Core::Instance ().GetROManager ()->CreateOpenMenu (this);
		connect (roMenu,
				SIGNAL (triggered (QAction*)),
				this,
				SLOT (handleRecentOpenAction (QAction*)));

		auto openButton = new QToolButton ();
		openButton->setDefaultAction (open);
		openButton->setMenu (roMenu);
		openButton->setPopupMode (QToolButton::MenuButtonPopup);
		Toolbar_->addWidget (openButton);

		auto print = new QAction (tr ("Print..."), this);
		print->setProperty ("ActionIcon", "document-print");
		connect (print,
				SIGNAL (triggered ()),
				this,
				SLOT (handlePrint ()));
		Toolbar_->addAction (print);

		auto presentation = new QAction (tr ("Presentation..."), this);
		presentation->setProperty ("ActionIcon", "view-presentation");
		connect (presentation,
				SIGNAL (triggered ()),
				this,
				SLOT (handlePresentation ()));
		Toolbar_->addAction (presentation);

		Toolbar_->addSeparator ();

		auto prev = new QAction (tr ("Previous page"), this);
		prev->setProperty ("ActionIcon", "go-previous-view-page");
		connect (prev,
				SIGNAL (triggered ()),
				this,
				SLOT (handleGoPrev ()));
		Toolbar_->addAction (prev);

		PageNumLabel_ = new QLineEdit ();
		PageNumLabel_->setMaximumWidth (fontMetrics ().width (" 1500 / 1500 "));
		PageNumLabel_->setAlignment (Qt::AlignCenter);
		connect (PageNumLabel_,
				SIGNAL (returnPressed ()),
				this,
				SLOT (navigateNumLabel ()));
		connect (Ui_.PagesView_->verticalScrollBar (),
				SIGNAL (valueChanged (int)),
				this,
				SLOT (checkCurrentPageChange ()));
		connect (Ui_.PagesView_->verticalScrollBar (),
				SIGNAL (valueChanged (int)),
				this,
				SIGNAL (tabRecoverDataChanged ()));
		connect (Ui_.PagesView_->verticalScrollBar (),
				SIGNAL (valueChanged (int)),
				this,
				SLOT (scheduleSaveState ()));
		Toolbar_->addWidget (PageNumLabel_);

		auto next = new QAction (tr ("Next page"), this);
		next->setProperty ("ActionIcon", "go-next-view-page");
		connect (next,
				SIGNAL (triggered ()),
				this,
				SLOT (handleGoNext ()));
		Toolbar_->addAction (next);

		Toolbar_->addSeparator ();

		ScalesBox_ = new QComboBox ();
		ScalesBox_->addItem (tr ("Fit width"));
		ScalesBox_->addItem (tr ("Fit page"));
		std::vector<double> scales = { 0.1, 0.25, 0.33, 0.5, 0.66, 0.8, 1.0, 1.25, 1.5, 2, 3, 4, 5, 7.5, 10 };
		Q_FOREACH (double scale, scales)
			ScalesBox_->addItem (QString::number (scale * 100) + '%', scale);
		ScalesBox_->setCurrentIndex (0);
		connect (ScalesBox_,
				SIGNAL (activated (int)),
				this,
				SLOT (handleScaleChosen (int)));
		Toolbar_->addWidget (ScalesBox_);

		ZoomOut_ = new QAction (tr ("Zoom out"), this);
		ZoomOut_->setProperty ("ActionIcon", "zoom-out");
		ZoomOut_->setShortcut (QString ("Ctrl+-"));
		connect (ZoomOut_,
				SIGNAL (triggered ()),
				this,
				SLOT (zoomOut ()));
		Toolbar_->addAction(ZoomOut_);

		ZoomIn_ = new QAction (tr ("Zoom in"), this);
		ZoomIn_->setProperty ("ActionIcon", "zoom-in");
		ZoomIn_->setShortcut (QString ("Ctrl+="));
		connect (ZoomIn_,
				SIGNAL (triggered ()),
				this,
				SLOT (zoomIn ()));
		Toolbar_->addAction (ZoomIn_);
		Toolbar_->addSeparator ();

		auto viewGroup = new QActionGroup (this);
		LayOnePage_ = new QAction (tr ("One page"), this);
		LayOnePage_->setProperty ("ActionIcon", "page-simple");
		LayOnePage_->setCheckable (true);
		LayOnePage_->setChecked (true);
		LayOnePage_->setActionGroup (viewGroup);
		connect (LayOnePage_,
				SIGNAL (triggered ()),
				this,
				SLOT (showOnePage ()));
		Toolbar_->addAction (LayOnePage_);

		LayTwoPages_ = new QAction (tr ("Two pages"), this);
		LayTwoPages_->setProperty ("ActionIcon", "page-2sides");
		LayTwoPages_->setCheckable (true);
		LayTwoPages_->setActionGroup (viewGroup);
		connect (LayTwoPages_,
				SIGNAL (triggered ()),
				this,
				SLOT (showTwoPages ()));
		Toolbar_->addAction (LayTwoPages_);

		Toolbar_->addSeparator ();

		auto mouseModeGroup = new QActionGroup (this);
		auto moveModeAction = new QAction (tr ("Move mode"), this);
		moveModeAction->setProperty ("ActionIcon", "transform-move");
		moveModeAction->setCheckable (true);
		moveModeAction->setChecked (true);
		moveModeAction->setActionGroup (mouseModeGroup);
		connect (moveModeAction,
				SIGNAL (triggered (bool)),
				this,
				SLOT (setMoveMode (bool)));
		Toolbar_->addAction (moveModeAction);

		auto selectModeAction = new QAction (tr ("Selection mode"), this);
		selectModeAction->setProperty ("ActionIcon", "edit-select");
		selectModeAction->setCheckable (true);
		selectModeAction->setActionGroup (mouseModeGroup);
		connect (selectModeAction,
				SIGNAL (triggered (bool)),
				this,
				SLOT (setSelectionMode (bool)));
		Toolbar_->addAction (selectModeAction);

		Toolbar_->addSeparator ();

		auto infoAction = new QAction (tr ("Document info..."), this);
		infoAction->setProperty ("ActionIcon", "dialog-information");
		connect (infoAction,
				SIGNAL (triggered ()),
				this,
				SLOT (showDocInfo ()));
		Toolbar_->addAction (infoAction);
	}

	QPoint DocumentTab::GetViewportCenter () const
	{
		return LayoutManager_->GetViewportCenter ();
	}

	void DocumentTab::Relayout ()
	{
		if (!CurrentDoc_)
			return;

		LayoutManager_->Relayout ();

		if (Onload_.Num_ >= 0)
		{
			handleNavigateRequested (QString (), Onload_.Num_, Onload_.X_, Onload_.Y_);
			Onload_.Num_ = -1;
		}

		checkCurrentPageChange (true);
	}

	QImage DocumentTab::GetSelectionImg ()
	{
		const auto& bounding = Scene_.selectionArea ().boundingRect ();
		if (bounding.isEmpty ())
			return QImage ();

		QImage image (bounding.size ().toSize (), QImage::Format_ARGB32);
		QPainter painter (&image);
		Scene_.render (&painter, QRectF (), bounding);
		painter.end ();
		return image;
	}

	QString DocumentTab::GetSelectionText () const
	{
		auto ihtc = qobject_cast<IHaveTextContent*> (CurrentDoc_->GetQObject ());
		if (!ihtc)
			return QString ();

		const auto& selectionBound = Scene_.selectionArea ().boundingRect ();

		auto bounding = Ui_.PagesView_->mapFromScene (selectionBound).boundingRect ();
		if (bounding.isEmpty () ||
				bounding.width () < 4 ||
				bounding.height () < 4)
		{
			qWarning () << Q_FUNC_INFO
					<< "selection area is empty";
			return QString ();
		}

		auto item = Ui_.PagesView_->itemAt (bounding.topLeft ());
		auto pageItem = dynamic_cast<PageGraphicsItem*> (item);
		if (!pageItem)
		{
			qWarning () << Q_FUNC_INFO
					<< "page item is null for"
					<< bounding.topLeft ();
			return QString ();
		}

		bounding = item->mapFromScene (selectionBound).boundingRect ().toRect ();

		const auto scale = LayoutManager_->GetCurrentScale ();
		bounding.moveTopLeft (bounding.topLeft () / scale);
		bounding.setSize (bounding.size () / scale);

		return ihtc->GetTextContent (pageItem->GetPageNum (), bounding);
	}

	void DocumentTab::RegenPageVisibility ()
	{
		if (receivers (SIGNAL (pagesVisibilityChanged (QMap<int, QRect>))) <= 0)
			return;

		const auto& viewRect = Ui_.PagesView_->viewport ()->rect ();
		const auto& visibleRect = Ui_.PagesView_->mapToScene (viewRect);

		QMap<int, QRect> rects;
		for (auto item : Ui_.PagesView_->items (viewRect))
		{
			auto page = dynamic_cast<PageGraphicsItem*> (item);
			if (!page)
				continue;

			const auto& pageRect = page->mapToScene (page->boundingRect ());
			const auto& xsect = visibleRect.intersected (pageRect);
			const auto& pageXsect = page->MapToDoc (page->mapFromScene (xsect).boundingRect ());
			rects [page->GetPageNum ()] = pageXsect.toAlignedRect ();
		}

		emit pagesVisibilityChanged (rects);
	}

	void DocumentTab::handleNavigateRequested (QString path, int num, double x, double y)
	{
		if (!path.isEmpty ())
		{
			if (QFileInfo (path).isRelative ())
				path = QFileInfo (CurrentDocPath_).dir ().absoluteFilePath (path);

			Onload_ = { num, x, y };

			if (!SetDoc (path))
				Onload_.Num_ = -1;
			return;
		}

		SetCurrentPage (num);

		auto page = Pages_.value (num);
		if (!page)
			return;

		if (x > 0 && y > 0)
		{
			const auto& size = page->boundingRect ().size ();
			const auto& mapped = page->mapToScene (size.width () * x, size.height () * y);
			Ui_.PagesView_->SmoothCenterOn (mapped.x (), mapped.y ());
		}
	}

	void DocumentTab::handleThumbnailClicked (int num)
	{
		SetCurrentPage (num);
	}

	void DocumentTab::handlePageContentsChanged (int idx)
	{
		auto pageItem = Pages_.at (idx);
		pageItem->UpdatePixmap ();
	}

	void DocumentTab::saveState ()
	{
		if (!SaveStateScheduled_)
			return;

		SaveStateScheduled_ = false;

		if (CurrentDocPath_.isEmpty ())
			return;

		const auto& filename = QFileInfo (CurrentDocPath_).fileName ();
		Core::Instance ().GetDocStateManager ()->SetState (filename,
				{
					GetCurrentPage (),
					LayoutManager_->GetLayoutMode (),
					LayoutManager_->GetCurrentScale (),
					LayoutManager_->GetScaleMode ()
				});
	}

	void DocumentTab::scheduleSaveState ()
	{
		if (SaveStateScheduled_)
			return;

		QTimer::singleShot (5000,
				this,
				SLOT (saveState ()));
		SaveStateScheduled_ = true;
	}

	void DocumentTab::handleRecentOpenAction (QAction *action)
	{
		const auto& path = action->property ("Path").toString ();
		const QFileInfo fi (path);
		if (!fi.exists ())
		{
			QMessageBox::warning (0,
					"LeechCraft",
					tr ("Seems like file %1 doesn't exist anymore.")
						.arg ("<em>" + fi.fileName () + "</em>"));
			return;
		}

		SetDoc (path);
	}

	void DocumentTab::selectFile ()
	{
		const auto& prevPath = XmlSettingsManager::Instance ()
				.Property ("LastOpenFileName", QDir::homePath ()).toString ();
		const auto& path = QFileDialog::getOpenFileName (this,
					tr ("Select file"),
					prevPath);
		if (path.isEmpty ())
			return;

		XmlSettingsManager::Instance ()
				.setProperty ("LastOpenFileName", QFileInfo (path).absolutePath ());

		SetDoc (path);
	}

	void DocumentTab::handlePrint ()
	{
		if (!CurrentDoc_)
			return;

		const int numPages = CurrentDoc_->GetNumPages ();

		QPrinter printer;
		QPrintDialog dia (&printer, this);
		dia.setMinMax (1, numPages);
		dia.addEnabledOption (QAbstractPrintDialog::PrintCurrentPage);
		if (dia.exec () != QDialog::Accepted)
			return;

		const auto& pageRect = printer.pageRect (QPrinter::Point);
		const auto& pageSize = pageRect.size ();
		const auto resScale = printer.resolution () / 72.0;

		const auto& range = dia.printRange ();
		int start = 0, end = 0;
		switch (range)
		{
		case QAbstractPrintDialog::AllPages:
			start = 0;
			end = numPages;
			break;
		case QAbstractPrintDialog::Selection:
			return;
		case QAbstractPrintDialog::PageRange:
			start = printer.fromPage () - 1;
			end = printer.toPage ();
			break;
		case QAbstractPrintDialog::CurrentPage:
			start = GetCurrentPage ();
			end = start + 1;
			if (start < 0)
				return;
			break;
		}

		QPainter painter (&printer);
		painter.setRenderHint (QPainter::Antialiasing);
		painter.setRenderHint (QPainter::HighQualityAntialiasing);
		painter.setRenderHint (QPainter::SmoothPixmapTransform);
		for (int i = start; i < end; ++i)
		{
			const auto& size = CurrentDoc_->GetPageSize (i);
			const auto scale = std::min (static_cast<double> (pageSize.width ()) / size.width (),
					static_cast<double> (pageSize.height ()) / size.height ());

			const auto& img = CurrentDoc_->RenderPage (i, resScale * scale, resScale * scale);
			painter.drawImage (0, 0, img);

			if (i != end - 1)
				printer.newPage ();
		}
		painter.end ();
	}

	void DocumentTab::handlePresentation ()
	{
		if (!CurrentDoc_)
			return;

		new PresenterWidget (CurrentDoc_);
	}

	void DocumentTab::handleGoPrev ()
	{
		SetCurrentPage (GetCurrentPage () - LayoutManager_->GetLayoutModeCount ());

		scheduleSaveState ();
	}

	void DocumentTab::handleGoNext ()
	{
		SetCurrentPage (GetCurrentPage () + LayoutManager_->GetLayoutModeCount ());

		scheduleSaveState ();
	}

	void DocumentTab::navigateNumLabel ()
	{
		auto text = PageNumLabel_->text ();
		const int pos = text.indexOf ('/');
		if (pos >= 0)
			text = text.left (pos - 1);

		SetCurrentPage (text.trimmed ().toInt () - 1);

		scheduleSaveState ();
	}

	void DocumentTab::updateNumLabel ()
	{
		if (!CurrentDoc_)
			return;

		const auto& str = QString::number (GetCurrentPage () + 1) +
				" / " +
				QString::number (CurrentDoc_->GetNumPages ());
		PageNumLabel_->setText (str);
	}

	void DocumentTab::checkCurrentPageChange (bool force)
	{
		RegenPageVisibility ();

		auto current = GetCurrentPage ();
		if (PrevCurrentPage_ == current && !force)
			return;

		PrevCurrentPage_ = current;
		emit currentPageChanged (current);
	}

	void DocumentTab::zoomOut ()
	{
		auto currentMatchingIndex = ScalesBox_->currentIndex ();
		const int minIdx = 2;
		switch (ScalesBox_->currentIndex ())
		{
		case 0:
		case 1:
		{
			const auto scale = LayoutManager_->GetCurrentScale ();
			for (auto i = minIdx; i < ScalesBox_->count (); ++i)
				if (ScalesBox_->itemData (i).toDouble () > scale)
				{
					currentMatchingIndex = i;
					break;
				}

			if (currentMatchingIndex == ScalesBox_->currentIndex ())
				currentMatchingIndex = ScalesBox_->count () - 1;
			break;
		}
		}

		auto newIndex = std::max (currentMatchingIndex - 1, minIdx);
		ScalesBox_->setCurrentIndex (newIndex);
		handleScaleChosen (newIndex);

		ZoomOut_->setEnabled (newIndex > minIdx);
		ZoomIn_->setEnabled (true);
	}

	void DocumentTab::zoomIn ()
	{
		const auto maxIdx = ScalesBox_->count () - 1;

		auto newIndex = std::min (ScalesBox_->currentIndex () + 1, maxIdx);
		switch (ScalesBox_->currentIndex ())
		{
		case 0:
		case 1:
			const auto scale = LayoutManager_->GetCurrentScale ();
			for (auto i = 2; i <= maxIdx; ++i)
				if (ScalesBox_->itemData (i).toDouble () > scale)
				{
					newIndex = i;
					break;
				}
			if (ScalesBox_->currentIndex () == newIndex)
				newIndex = maxIdx;
			break;
		}

		ScalesBox_->setCurrentIndex (newIndex);
		handleScaleChosen (newIndex);

		ZoomOut_->setEnabled (true);
		ZoomIn_->setEnabled (newIndex < maxIdx);
	}

	void DocumentTab::showOnePage ()
	{
		LayoutManager_->SetLayoutMode (LayoutMode::OnePage);
		Relayout ();

		emit tabRecoverDataChanged ();

		scheduleSaveState ();
	}

	void DocumentTab::showTwoPages ()
	{
		LayoutManager_->SetLayoutMode (LayoutMode::TwoPages);
		Relayout ();

		emit tabRecoverDataChanged ();

		scheduleSaveState ();
	}

	void DocumentTab::syncUIToLayMode ()
	{
		auto action = LayoutManager_->GetLayoutMode () == LayoutMode::OnePage ?
				LayOnePage_ :
				LayTwoPages_;
		action->setChecked (true);
	}

	void DocumentTab::recoverDocState (DocStateManager::State state)
	{
		if (state.CurrentScale_ <= 0)
			return;

		LayoutManager_->SetLayoutMode (state.Lay_);
		LayoutManager_->SetScaleMode (state.ScaleMode_);
		LayoutManager_->SetFixedScale (state.CurrentScale_);

		switch (state.ScaleMode_)
		{
		case ScaleMode::FitWidth:
			ScalesBox_->setCurrentIndex (0);
			break;
		case ScaleMode::FitPage:
			ScalesBox_->setCurrentIndex (1);
			break;
		case ScaleMode::Fixed:
		{
			const auto scaleIdx = ScalesBox_->findData (state.CurrentScale_);
			if (scaleIdx >= 0)
				ScalesBox_->setCurrentIndex (scaleIdx);
			break;
		}
		}

		syncUIToLayMode ();
	}

	void DocumentTab::setMoveMode (bool enable)
	{
		if (!enable)
			return;

		MouseMode_ = MouseMode::Move;
		Ui_.PagesView_->SetShowReleaseMenu (false);
		Ui_.PagesView_->setDragMode (QGraphicsView::ScrollHandDrag);
	}

	void DocumentTab::setSelectionMode (bool enable)
	{
		if (!enable)
			return;

		MouseMode_ = MouseMode::Select;
		Ui_.PagesView_->SetShowReleaseMenu (true);
		Ui_.PagesView_->setDragMode (QGraphicsView::RubberBandDrag);
	}

	void DocumentTab::handleCopyAsImage ()
	{
		QApplication::clipboard ()->setImage (GetSelectionImg ());
	}

	void DocumentTab::handleSaveAsImage ()
	{
		const auto& image = GetSelectionImg ();
		if (image.isNull ())
			return;

		const auto& previous = XmlSettingsManager::Instance ()
				.Property ("SelectionImageSavePath", QDir::homePath ()).toString ();
		const auto& filename = QFileDialog::getSaveFileName (this,
				tr ("Save selection as"),
				previous,
				tr ("PNG images (*.png)"));
		if (filename.isEmpty ())
			return;

		const QFileInfo saveFI (filename);
		XmlSettingsManager::Instance ().setProperty ("SelectionImageSavePath",
				saveFI.absoluteFilePath ());
		const auto& userSuffix = saveFI.suffix ().toLatin1 ();
		const auto& supported = QImageWriter::supportedImageFormats ();
		const auto suffix = supported.contains (userSuffix) ?
				userSuffix :
				QByteArray ("PNG");
		image.save (filename, suffix, 100);
	}

	void DocumentTab::handleCopyAsText ()
	{
		const auto& text = sender ()->property ("Monocle/Text").toString ();
		QApplication::clipboard ()->setText (text);
	}

	void DocumentTab::showDocInfo ()
	{
		if (!CurrentDoc_)
			return;

		auto dia = new DocInfoDialog (CurrentDocPath_, CurrentDoc_->GetDocumentInfo (), this);
		dia->setAttribute (Qt::WA_DeleteOnClose);
		dia->show ();
	}

	void DocumentTab::delayedCenterOn (const QPoint& point)
	{
		Ui_.PagesView_->SmoothCenterOn (point.x (), point.y ());
	}

	void DocumentTab::handleScaleChosen (int idx)
	{
		switch (idx)
		{
		case 0:
			LayoutManager_->SetScaleMode (ScaleMode::FitWidth);
			break;
		case 1:
			LayoutManager_->SetScaleMode (ScaleMode::FitPage);
			break;
		default:
			LayoutManager_->SetScaleMode (ScaleMode::Fixed);
			LayoutManager_->SetFixedScale (ScalesBox_->itemData (idx).toDouble ());
			break;
		}

		Relayout ();

		scheduleSaveState ();

		emit tabRecoverDataChanged ();
	}

	void DocumentTab::handleDockLocation (Qt::DockWidgetArea area)
	{
		if (area != Qt::AllDockWidgetAreas &&
				area != Qt::NoDockWidgetArea)
			XmlSettingsManager::Instance ().setProperty ("DockWidgetArea", area);
	}
}
}
