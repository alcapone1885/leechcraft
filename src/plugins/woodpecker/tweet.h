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

#ifndef TWEET_H
#define TWEET_H

#include <memory>
#include <QObject>
#include <QDateTime>
#include <QTextLayout>
#include <QListWidget>
#include <QTextDocument>
#include "twitteruser.h"

namespace LeechCraft
{
namespace Woodpecker
{

class Tweet : public QObject
{
	Q_OBJECT

public:
	Tweet (QObject *parent = 0);
	Tweet (QString text, TwitterUser *author = 0, QObject *parent = 0);
    Tweet (const Tweet& original);
	~Tweet ();
	void setText (QString text);

	QString text () const {
		return m_text;
	}
	
	qulonglong id () const {
		return m_id;
	}
	void setId (qulonglong id) {
		m_id = id;
	}

	TwitterUser* author () const {
		return m_author;
	}
	void setAuthor (TwitterUser *newAuthor) {
		m_author = newAuthor;
	}

	QDateTime dateTime () const {
		return m_created;
	}
	
	void setDateTime (QDateTime datetime) {
		m_created = datetime;
	}
	
	QTextDocument* getDocument() {
		return &m_document;
	}
	
	Tweet& operator= (const Tweet&);
	bool operator== (const Tweet&);
	bool operator!= (const Tweet&);
	bool operator> (const Tweet&);
	bool operator< (const Tweet&);

private:
	qulonglong	m_id;
	QString		m_text;
	TwitterUser	*m_author;
	QDateTime	m_created;
	QTextDocument m_document;

signals:

public slots:

};
}
}

Q_DECLARE_METATYPE ( LeechCraft::Woodpecker::Tweet );
Q_DECLARE_METATYPE ( std::shared_ptr<LeechCraft::Woodpecker::Tweet> );

#endif // TWEET_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
