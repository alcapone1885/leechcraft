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

#include <QObject>
#include <QHash>
#include <QMap>
#include <QStringList>

typedef struct _XDisplay Display;

namespace LeechCraft
{
namespace KBSwitch
{
	class RulesStorage : public QObject
	{
		Display * const Display_;
		const QString X11Dir_;

		QHash<QString, QString> LayName2Desc_;
		QHash<QString, QString> LayDesc2Name_;

		QHash<QString, QStringList> LayName2Variants_;
		QHash<QString, QPair<QString, QString>> VarLayHR2NameVarPair_;

		QHash<QString, QString> KBModels_;
		QHash<QString, QString> KBModelString2Code_;
		QStringList KBModelsStrings_;

		QMap<QString, QMap<QString, QString>> Options_;
	public:
		RulesStorage (Display*, QObject* = 0);

		const QHash<QString, QString>& GetLayoutsD2N () const;
		const QHash<QString, QString>& GetLayoutsN2D () const;

		const QHash<QString, QPair<QString, QString>>& GetVariantsD2Layouts () const;

		QStringList GetLayoutVariants (const QString&) const;

		const QHash<QString, QString>& GetKBModels () const;
		const QStringList& GetKBModelsStrings () const;
		QString GetKBModelCode (const QString&) const;

		const QMap<QString, QMap<QString, QString>>& GetOptions () const;
	};
}
}
