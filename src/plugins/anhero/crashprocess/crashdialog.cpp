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

#include "crashdialog.h"
#include <QFileDialog>
#include <QDateTime>
#include <QMessageBox>

namespace LeechCraft
{
namespace AnHero
{
namespace CrashProcess
{
	CrashDialog::CrashDialog (QWidget *parent)
	: QDialog (parent, Qt::Window)
	{
		Ui_.setupUi (this);
		Ui_.InfoLabel_->setText (tr ("Unfortunately LeechCraft has crashed. This is the info we could collect:"));
		show ();
	}

	void CrashDialog::accept ()
	{
		QDialog::accept ();
	}

	void CrashDialog::appendTrace (const QString& part)
	{
		Ui_.TraceDisplay_->append (part);
	}

	void CrashDialog::on_Save__released ()
	{
		const auto& nowStr = QDateTime::currentDateTime ().toString ("yy_MM_dd-hh_mm_ss");
		const auto& filename = QFileDialog::getSaveFileName (this,
				"Save crash info",
				QDir::homePath () + "/lc_crash_" + nowStr + ".log");

		if (filename.isEmpty ())
			return;

		QFile file (filename);
		if (!file.open (QIODevice::WriteOnly | QIODevice::Truncate))
		{
			QMessageBox::critical (this,
					"LeechCraft",
					tr ("Cannot open file: %1")
						.arg (file.errorString ()));
			return;
		}
		file.write (Ui_.TraceDisplay_->toPlainText ().toUtf8 ());
	}
}
}
}
