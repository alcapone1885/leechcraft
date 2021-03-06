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
#include <QUrl>
#include <QTime>
#include <QNetworkReply>
#include <QStringList>
#include <interfaces/structures.h>

class QAuthenticator;
class QNetworkProxy;
class QIODevice;
class QFile;
class QTimer;

namespace LeechCraft
{
namespace CSTP
{
	class Task : public QObject
	{
		Q_OBJECT

		std::unique_ptr<QNetworkReply, std::function<void (QNetworkReply*)>> Reply_;
		QUrl URL_;
		QTime StartTime_;
		qint64 Done_, Total_, FileSizeAtStart_;
		double Speed_;
		QList<QByteArray> RedirectHistory_;
		std::shared_ptr<QFile> To_;
		int UpdateCounter_;
		QTimer *Timer_;
		bool CanChangeName_;

		QUrl Referer_;
		const QVariantMap Params_;
	public:
		explicit Task (const QUrl& url = QUrl (), const QVariantMap& params = QVariantMap ());
		explicit Task (QNetworkReply*);

		void Start (const std::shared_ptr<QFile>&);
		void Stop ();
		void ForbidNameChanges ();

		QByteArray Serialize () const;
		void Deserialize (QByteArray&);

		double GetSpeed () const;
		qint64 GetDone () const;
		qint64 GetTotal () const;
		QString GetState () const;
		QString GetURL () const;
		int GetTimeFromStart () const;
		bool IsRunning () const;
		QString GetErrorString () const;
	private:
		void Reset ();
		void RecalculateSpeed ();
		void HandleMetadataRedirection ();
		void HandleMetadataFilename ();

		void Cleanup ();
	private slots:
		void handleDataTransferProgress (qint64, qint64);
		void redirectedConstruction (const QByteArray&);
		void handleMetaDataChanged ();
		void handleLocalTransfer ();
		/** Returns true if the reply is at end after this read.
			*/
		bool handleReadyRead ();
		void handleFinished ();
		void handleError ();
	signals:
		void gotEntity (const LeechCraft::Entity&);
		void updateInterface ();
		void done (bool);
	};
}
}
