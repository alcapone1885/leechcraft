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

#include "imagecache.h"
#include <QDesktopServices>
#include <QImage>
#include <QUrl>
#include <QWebPage>
#include <QWebFrame>
#include <QPainter>
#include <util/sys/paths.h>

namespace LeechCraft
{
namespace Poshuku
{
namespace SpeedDial
{
	const QSize ThumbSize { 144, 90 };
	const QSize RenderSize = ThumbSize * 10;

	ImageCache::ImageCache ()
	: CacheDir_ { Util::GetUserDir (Util::UserDir::Cache, "poshuku/speeddial/snapshots") }
	{
	}

	QImage ImageCache::GetSnapshot (const QUrl& url)
	{
		const auto& filename = QString::number (qHash (url)) + ".png";
		if (CacheDir_.exists (filename))
		{
			QImage result { CacheDir_.filePath (filename) };
			if (!result.isNull ())
				return result;
		}

		if (Url2Page_.contains (url))
			return {};

		const auto page = new QWebPage;
		connect (page,
				SIGNAL (loadFinished (bool)),
				this,
				SLOT (handleLoadFinished ()));
		Page2Url_ [page] = url;
		Url2Page_ [url] = page;
		page->mainFrame ()->load (url);

		return {};
	}

	QSize ImageCache::GetThumbSize () const
	{
		return ThumbSize;
	}

	void ImageCache::handleLoadFinished ()
	{
		const auto page = qobject_cast<QWebPage*> (sender ());

		const auto& url = Page2Url_.take (page);
		Url2Page_.remove (url);

		page->setViewportSize (RenderSize);
		QImage image { page->viewportSize (), QImage::Format_ARGB32 };
		QPainter painter { &image };

		page->mainFrame ()->render (&painter);
		painter.end ();

		page->deleteLater ();

		const auto& thumb = image.scaled (ThumbSize,
				Qt::KeepAspectRatio, Qt::SmoothTransformation);
		thumb.save (CacheDir_.filePath (QString::number (qHash (url))) + ".png");

		emit gotSnapshot (url, thumb);
	}
}
}
}
