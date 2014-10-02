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

#include "toxaccountconfiguration.h"
#include <QDataStream>
#include <QtDebug>

namespace LeechCraft
{
namespace Azoth
{
namespace Sarin
{
	ToxAccountConfiguration::ToxAccountConfiguration (bool udp, bool ipv6, const QString& host, int port)
	: AllowUDP_ { udp }
	, AllowIPv6_ { ipv6 }
	, ProxyHost_ { host }
	, ProxyPort_ { port }
	{
	}

	bool operator== (const ToxAccountConfiguration& c1, const ToxAccountConfiguration& c2)
	{
		return c1.AllowUDP_ == c2.AllowUDP_ &&
				c1.AllowIPv6_ == c2.AllowIPv6_ &&
				c1.ProxyPort_ == c2.ProxyPort_ &&
				c1.ProxyHost_ == c2.ProxyHost_;
	}

	bool operator!= (const ToxAccountConfiguration& c1, const ToxAccountConfiguration& c2)
	{
		return !(c1 == c2);
	}

	QDataStream& operator<< (QDataStream& out, const ToxAccountConfiguration& config)
	{
		out << static_cast<quint8> (1)
				<< config.AllowUDP_
				<< config.AllowIPv6_
				<< config.ProxyHost_
				<< config.ProxyPort_;
		return out;
	}

	QDataStream& operator>> (QDataStream& in, ToxAccountConfiguration& config)
	{
		quint8 version = 0;
		in >> version;
		if (version != 1)
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown version"
					<< version;
			return in;
		}

		in >> config.AllowUDP_
				>> config.AllowIPv6_
				>> config.ProxyHost_
				>> config.ProxyPort_;
		return in;
	}
}
}
}
