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

#include "toxthread.h"
#include <QElapsedTimer>
#include <QMutexLocker>
#include <QtEndian>
#include <QtDebug>
#include <tox/tox.h>

namespace LeechCraft
{
namespace Azoth
{
namespace Sarin
{
	ToxThread::ToxThread (const QString& name, const QByteArray& state)
	: Name_ { name }
	, ToxState_ { state }
	{
	}

	ToxThread::~ToxThread ()
	{
		if (!isRunning ())
			return;

		ShouldStop_ = true;

		wait (2000);
		if (isRunning ())
			terminate ();
	}

	namespace
	{
		template<typename T>
		void DoTox (const QString& str, T f)
		{
			const auto& strUtf8 = str.toUtf8 ();
			f (reinterpret_cast<const uint8_t*> (strUtf8.constData ()), strUtf8.size ());
		}

		void SetToxStatus (Tox *tox, const EntryStatus& status)
		{
			DoTox (status.StatusString_,
					[tox] (auto bytes, auto size) { tox_set_status_message (tox, bytes, size); });
			int toxStatus = TOX_USERSTATUS_NONE;
			switch (status.State_)
			{
			case SAway:
			case SXA:
				toxStatus = TOX_USERSTATUS_AWAY;
				break;
			case SDND:
				toxStatus = TOX_USERSTATUS_BUSY;
				break;
			default:
				break;
			}
			tox_set_user_status (tox, toxStatus);
		}
	}

	EntryStatus ToxThread::GetStatus () const
	{
		return Status_;
	}

	void ToxThread::SetStatus (const EntryStatus& status)
	{
		Status_ = status;
		if (IsStoppable ())
			ScheduleFunction ([status, this] (Tox *tox)
					{
						SetToxStatus (tox, status);
						emit statusChanged (status);
					});
	}

	void ToxThread::Stop ()
	{
		ShouldStop_ = true;
	}

	bool ToxThread::IsStoppable () const
	{
		return isRunning () && !ShouldStop_;
	}

	void ToxThread::ScheduleFunction (const std::function<void (Tox*)>& function)
	{
		QMutexLocker locker { &FQueueMutex_ };
		FQueue_ << function;
	}

	void ToxThread::SaveState ()
	{
		const auto size = tox_size (Tox_.get ());
		if (!size)
			return;

		QByteArray newState { static_cast<int> (size), 0 };
		tox_save (Tox_.get (), reinterpret_cast<uint8_t*> (newState.data ()));

		if (newState == ToxState_)
			return;

		ToxState_ = newState;
		emit toxStateChanged (ToxState_);
	}

	QByteArray ToxThread::GetToxAddress () const
	{
		if (!Tox_)
			return {};

		QByteArray result;

		uint8_t address [TOX_FRIEND_ADDRESS_SIZE];
		tox_get_address (Tox_.get (), address);

		auto toHexChar = [] (uint8_t num) -> char
		{
			return num >= 10 ? (num - 10 + 'A') : (num + '0');
		};

		for (size_t i = 0; i < TOX_FRIEND_ADDRESS_SIZE; ++i)
		{
			const auto num = address [i];
			result += toHexChar ((num & 0xf0) >> 4);
			result += toHexChar (num & 0xf);
		}

		return result;
	}

	namespace
	{
		QByteArray HexStringToBin (const QByteArray& key)
		{
			return QByteArray::fromHex (key.toLower ());
		}
	}

	void ToxThread::run ()
	{
		qDebug () << Q_FUNC_INFO;
		Tox_ = std::shared_ptr<Tox> { tox_new (TOX_ENABLE_IPV6_DEFAULT), &tox_kill };

		DoTox (Name_,
				[this] (const uint8_t *bytes, uint16_t size) { tox_set_name (Tox_.get (), bytes, size); });

		SetToxStatus (Tox_.get (), Status_);

		if (!ToxState_.isEmpty ())
		{
			const auto res = tox_load (Tox_.get (),
					reinterpret_cast<const uint8_t*> (ToxState_.constData ()),
					ToxState_.size ());
			if (!res)
				qDebug () << "successfully loaded Tox state";
			else
				qWarning () << "failed to load Tox state";
		}

		qDebug () << "gonna bootstrap..." << Tox_.get ();
		const auto pubkey = HexStringToBin ("F404ABAA1C99A9D37D61AB54898F56793E1DEF8BD46B1038B9D822E8460FAB67");
		tox_bootstrap_from_address (Tox_.get (),
				"192.210.149.121",
				0,
				qToBigEndian (static_cast<uint16_t> (33445)),
				reinterpret_cast<const uint8_t*> (pubkey.constData ()));

		bool wasConnected = false;
		while (!ShouldStop_)
		{
			tox_do (Tox_.get ());
			auto next = tox_do_interval (Tox_.get ());

			if (!wasConnected && tox_isconnected (Tox_.get ()))
			{
				wasConnected = true;
				qDebug () << "connected! tox id is" << GetToxAddress ();

				emit statusChanged (Status_);

				SaveState ();
			}

			QElapsedTimer timer;
			timer.start ();
			decltype (FQueue_) queue;
			{
				QMutexLocker locker { &FQueueMutex_ };
				std::swap (queue, FQueue_);
			}
			for (const auto item : queue)
				item (Tox_.get ());

			next -= timer.elapsed ();

			if (next > 0)
			{
				if (next > 100)
					next = 50;

				msleep (next);
			}
		}

		SaveState ();
	}
}
}
}
