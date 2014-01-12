/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
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

#include "afcfile.h"
#include <QtDebug>
#include "connection.h"

namespace LeechCraft
{
namespace LMP
{
namespace jOS
{
	AfcFile::AfcFile (const QString& path, Connection *conn, QObject *parent)
	: QIODevice { parent }
	, Conn_ { conn }
	, AFC_ { conn->GetAFC () }
	, Path_ { path }
	{
	}

	AfcFile::~AfcFile ()
	{
		close ();
	}

	namespace
	{
		afc_file_mode_t Qio2Afc (QIODevice::OpenMode mode)
		{
			switch (mode)
			{
			case QIODevice::ReadOnly:
				return AFC_FOPEN_RDONLY;
			case QIODevice::WriteOnly:
				return AFC_FOPEN_WRONLY;
			case QIODevice::ReadWrite:
			default:
				return AFC_FOPEN_RW;
			}
		}
	}

	bool AfcFile::open (QIODevice::OpenMode mode)
	{
		if (const auto err = afc_file_open (AFC_, Path_.toUtf8 ().constData (), Qio2Afc (mode), &Handle_))
		{
			const auto& str = tr ("Error opening file %1 in mode %2: %3.")
					.arg (Path_)
					.arg (mode)
					.arg (err);
			setErrorString (str);
			qWarning () << Q_FUNC_INFO
					<< str;
			return false;
		}

		return QIODevice::open (mode);
	}

	void AfcFile::close()
	{
		if (Handle_)
		{
			afc_file_close (AFC_, Handle_);
			Handle_ = 0;
		}

		QIODevice::close ();
	}

	bool AfcFile::seek (qint64 pos)
	{
		if (const auto err = afc_file_seek (AFC_, Handle_, pos, SEEK_SET))
		{
			const auto& str = tr ("Error seeking in file %1 to %2: %3.")
					.arg (Path_)
					.arg (pos)
					.arg (err);
			setErrorString (str);
			qWarning () << Q_FUNC_INFO
					<< str;
			return false;
		}

		QIODevice::seek (pos);
		return true;
	}

	qint64 AfcFile::size () const
	{
		return Conn_->GetFileInfo (Path_, "st_size").toLongLong ();
	}

	qint64 AfcFile::readData (char *data, qint64 maxlen)
	{
		uint32_t bytesRead = 0;
		if (const auto err = afc_file_read (AFC_, Handle_, data, maxlen, &bytesRead))
		{
			const auto& str = tr ("Error reading file %1: %2.")
					.arg (Path_)
					.arg (err);
			setErrorString (str);
			qWarning () << Q_FUNC_INFO
					<< str;
			return -1;
		}

		return bytesRead;
	}

	qint64 AfcFile::writeData (const char *data, qint64 len)
	{
		uint32_t bytesWritten = 0;
		if (const auto err = afc_file_write (AFC_, Handle_, data, len, &bytesWritten))
		{
			const auto& str = tr ("Error writing to file %1: %2.")
					.arg (Path_)
					.arg (err);
			setErrorString (str);
			qWarning () << Q_FUNC_INFO
					<< str;
			return -1;
		}

		return bytesWritten;
	}
}
}
}