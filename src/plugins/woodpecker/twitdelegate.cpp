/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2013  Slava Barinov <rayslava@gmail.com>
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

#include "twitdelegate.h"
#include <QDebug>
#include <QUrl>
#include <QPushButton>
#include <QApplication>
#include <QMouseEvent>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QRectF>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/structures.h>
#include <util/util.h>
#include "core.h"
#include "tweet.h"
#include <qt4/QtGui/QStyleOption>

namespace LeechCraft
{
namespace Woodpecker
{
	const int ImageSpace_ = 50;
	const int IconSize = 48;
	const int Padding = 5;
	
	TwitDelegate::TwitDelegate (QObject *parent)
	: QStyledItemDelegate (parent)
	{
	}
	
	void TwitDelegate::paint (QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
	{
		const QStyleOptionViewItemV4 o = option;
		QStyle *style = o.widget ?
				o.widget->style () :
				QApplication::style ();

		auto r = o.rect;
		const int maxIconHeight = r.height () - Padding * 2;

		const QPen linePen (o.palette.color (QPalette::AlternateBase), 1, Qt::SolidLine);
		QPen lineMarkedPen (o.palette.color (QPalette::Mid) , 1, Qt::SolidLine);
		QPen fontPen (o.palette.color (QPalette::Text), 1, Qt::SolidLine);
		QPen fontMarkedPen (o.palette.color (QPalette::HighlightedText), 1, Qt::SolidLine);

		QFont mainFont;
		mainFont.setFamily (mainFont.defaultFamily ());
		//mainFont.setPixelSize(10);

		const auto& bgBrush = QBrush (o.palette.color(QPalette::Base));
		const auto& selBgBrush = QBrush (o.palette.color(QPalette::Highlight));
		
		if (option.state & QStyle::State_Selected)
		{
			painter->setBrush (selBgBrush);
			painter->drawRect (r);

			// Border
			painter->setPen (lineMarkedPen);
			painter->drawLine (r.topLeft (), r.topRight ());
			painter->drawLine (r.topRight (), r.bottomRight ());
			painter->drawLine (r.bottomLeft (), r.bottomRight ());
			painter->drawLine (r.topLeft (), r.bottomLeft ());

			painter->setPen (fontMarkedPen);
		} 
		else 
		{
			// Background
			// Alternating colors
			
			painter->setBrush (bgBrush);
			painter->drawRect (r);

			// border
			painter->setPen (linePen);
			painter->drawLine (r.topLeft (), r.topRight ());
			painter->drawLine (r.topRight (), r.bottomRight ());
			painter->drawLine (r.bottomLeft (), r.bottomRight ());
			painter->drawLine (r.topLeft (), r.bottomLeft ());
			
			painter->setPen (fontPen);
		}
		
		// Get title, description and icon
		auto currentTweet = index.data (Qt::UserRole).value<std::shared_ptr<Tweet>> ();
		if (!currentTweet) 
		{
			qDebug () << "Can't recieve twit";
			return;
		}
		const qulonglong id = currentTweet->GetId ();
		const auto& author = currentTweet->GetAuthor ()->GetUsername ();
		const auto& time = currentTweet->GetDateTime ().toString ();
		QTextDocument* doc = currentTweet->GetDocument ();

		painter->setRenderHints (QPainter::HighQualityAntialiasing | QPainter::Antialiasing);
		// Icon
		QIcon ic = QIcon (index.data (Qt::DecorationRole).value<QPixmap>());
		if (!ic.isNull ()) 
		{
			r = option.rect.adjusted (Padding, Padding * 2, -Padding * 2, -Padding * 2);
			if (r.width () > IconSize || r.height () > IconSize)
				r.adjust(0, 0, -(r.width () - IconSize), -(r.height () - IconSize));
			ic.paint (painter, r, Qt::AlignVCenter | Qt::AlignLeft);
		}

		// Text
		r = option.rect.adjusted (ImageSpace_ + Padding, Padding, -Padding, -Padding);
		painter->setFont (mainFont);
		doc->setTextWidth (r.width ());
		painter->save ();
		QAbstractTextDocumentLayout::PaintContext ctx;
		ctx.palette.setColor (QPalette::Text, painter->pen ().color ());
		painter->translate (r.left (), r.top ());
		doc->documentLayout ()->draw (painter, ctx);
		painter->restore ();

		// Author
		r = option.rect.adjusted (ImageSpace_ + Padding, r.height() - mainFont.pixelSize() - Padding * 2, -Padding * 2, 0);
		auto author_rect = std::unique_ptr<QRect> (new QRect (r.left () + Padding, r.bottom () - painter->fontMetrics ().height () - 8, painter->fontMetrics ().width (author), r.height ()));
		painter->setFont (mainFont);
		painter->drawText (*(author_rect), Qt::AlignLeft, author, &r);

		// Time
		r = option.rect.adjusted (ImageSpace_ + Padding, Padding, -Padding * 2, -Padding);
		painter->setFont (mainFont);
		painter->drawText (r.right () - painter->fontMetrics ().width (time), 
						   r.bottom () - painter->fontMetrics ().height (),
						   r.width (), 
						   r.height (), 
						   Qt::AlignLeft, time, &r);
		painter->setPen (linePen);
	}

	QSize TwitDelegate::sizeHint (const QStyleOptionViewItem& option, const QModelIndex& index) const
	{
		QSize result = QStyledItemDelegate::sizeHint (option, index);
		QFontMetrics fm(option.font);
		const auto currentTweet = index.data (Qt::UserRole).value<Tweet_ptr> ();
		result.setHeight (std::max (
			qRound (currentTweet->GetDocument ()->documentLayout()->documentSize ().height () +
					fm.height () + Padding * 3), 
					IconSize + Padding * 2));
		return result;
	}

	bool TwitDelegate::editorEvent (QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem& option, const QModelIndex& index)
	{
		QListWidget *parentWidget = qobject_cast<QListWidget*> (parent ());

		if (event->type () == QEvent::MouseButtonRelease) 
		{
			const QMouseEvent *me = static_cast<QMouseEvent*> (event);
			if (me)
			{
				const auto currentTweet = index.data (Qt::UserRole).value<Tweet_ptr> ();
				const auto position = (me->pos () - option.rect.adjusted (ImageSpace_ + 14, 4, 0, -22).topLeft ());

				const QTextDocument *textDocument = currentTweet->GetDocument ();
				const int textCursorPosition =
					textDocument->documentLayout ()->hitTest (position, Qt::FuzzyHit);
				const QChar character (textDocument->characterAt (textCursorPosition));
				const QString string (character);

				const auto anchor = textDocument->documentLayout ()->anchorAt (position);

				if (parentWidget && !anchor.isEmpty ())
				{
					Entity url = Util::MakeEntity (QUrl (anchor), QString (), OnlyHandle | FromUserInitiated, QString ());
					Core::Instance ().GetCoreProxy ()->GetEntityManager ()->HandleEntity (url);
				}
			}
		}
		else if (event->type () == QEvent::MouseMove)
		{
			const QMouseEvent *me = static_cast<QMouseEvent*> (event);
			if (me)
			{
				const auto currentTweet = index.data (Qt::UserRole).value<std::shared_ptr<Tweet>> ();
				const auto position = (me->pos () - option.rect.adjusted (ImageSpace_ + 14, 4, 0, -22).topLeft ());

				const auto anchor = currentTweet->GetDocument ()->documentLayout ()->anchorAt (position);

				if (parentWidget) 
				{
					if (!anchor.isEmpty ())
						parentWidget->setCursor (Qt::PointingHandCursor);
					else
						parentWidget->unsetCursor ();
				}
			}
		}
		return QAbstractItemDelegate::editorEvent (event, model, option, index);
	}
}
}
