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

#include "commandstest.h"
#include <QtTest>
#include <QtDebug>
#include "presencecommand.cpp"

QTEST_MAIN (LeechCraft::Azoth::MuCommands::CommandsTest)

namespace
{
	struct PrintVisitor : boost::static_visitor<QString>
	{
		QString operator() (const std::string& str) const
		{
			return QString::fromStdString (str);
		}

		template<typename T>
		QString operator() (const T& t) const
		{
			QString result;
			QDebug { &result } << t;
			return result;
		}

		template<typename T1, typename T2>
		QString operator() (const std::pair<T1, T2>& t) const
		{
			QString result;
			QDebug { &result } << "std::pair { " << (*this) (t.first) << "; " << (*this) (t.second) << " }";
			return result;
		}

		template<typename... Args>
		QString operator() (const boost::variant<Args...>& variant) const
		{
			auto result = QString { "Variant with type %1, value: { %2 }" }
					.arg (variant.which ())
					.arg (boost::apply_visitor (PrintVisitor {}, variant));
			return result;
		}
	};

	template<typename... Args>
	char* PrintVar (const boost::variant<Args...>& variant)
	{
		const auto& result = PrintVisitor {} (variant);
		return qstrdup (result.toUtf8 ().constData ());
	}
}

namespace QTest
{
	template<>
	char* toString (const LeechCraft::Azoth::MuCommands::Status_t& acc)
	{
		return PrintVar (acc);
	}
}

namespace LeechCraft
{
namespace Azoth
{
namespace MuCommands
{
	void CommandsTest::basicTest ()
	{
		const QString command = R"delim(
testacc
away
				)delim";
		const auto& res = ParsePresenceCommand (command);
		QVERIFY (res.AccName_);
		QCOMPARE (QString::fromStdString (*res.AccName_), QString { "testacc" });
		QCOMPARE (res.Status_, Status_t { State_t { State::SAway } });
	}
}
}
}
