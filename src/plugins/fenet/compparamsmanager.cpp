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

#include "compparamsmanager.h"
#include <QStandardItemModel>
#include <QCoreApplication>
#include <QSettings>
#include <QtDebug>

namespace LeechCraft
{
namespace Fenet
{
	CompParamsManager::CompParamsManager (QObject *parent)
	: QObject (parent)
	, ParamsModel_ (new QStandardItemModel (this))
	{
		ParamsModel_->setHorizontalHeaderLabels ({ tr ("Option"), tr ("Value"), tr ("Flag") });
		connect (ParamsModel_,
				SIGNAL (itemChanged (QStandardItem*)),
				this,
				SLOT (handleItemChanged (QStandardItem*)));
	}

	QAbstractItemModel* CompParamsManager::GetModel () const
	{
		return ParamsModel_;
	}

	void CompParamsManager::SetCompInfo (const CompInfo& info)
	{
		if (auto rc = ParamsModel_->rowCount ())
			ParamsModel_->removeRows (0, rc);

		CurrentInfo_ = info;
		auto settings = GetSettings ();

		for (const auto& param : CurrentInfo_.Params_)
		{
			auto descItem = new QStandardItem (param.Desc_);
			descItem->setEditable (false);

			const auto value = settings->value (param.Name_, param.Default_).toDouble ();
			const auto valueItem = new QStandardItem (QString::number (value));
			valueItem->setData (value, Qt::EditRole);
			valueItem->setData (QVariant::fromValue (param), Role::Description);

			auto nameItem = new QStandardItem (param.Name_);

			ParamsModel_->appendRow ({ descItem, valueItem, nameItem });
		}

		const auto& enabledFlags = settings->value ("__Flags").toStringList ();

		for (const auto& flag : CurrentInfo_.Flags_)
		{
			auto descItem = new QStandardItem (flag.Desc_);
			descItem->setEditable (false);
			descItem->setCheckable (true);
			descItem->setCheckState (enabledFlags.contains (flag.Name_) ?
					Qt::Checked :
					Qt::Unchecked);
			descItem->setData (QVariant::fromValue (flag), Role::Description);

			auto valueItem = new QStandardItem ();
			valueItem->setEditable (false);

			auto nameItem = new QStandardItem (flag.Name_);
			nameItem->setEditable (false);

			ParamsModel_->appendRow ({ descItem, valueItem, nameItem });
		}
	}

	QStringList CompParamsManager::GetCompParams (const QString& compName) const
	{
		auto settings = GetSettings (compName);

		auto result = settings->value ("__Flags").toStringList ();
		for (const auto& key : settings->childKeys ())
		{
			if (key == "__Flags")
				continue;

			result << key << QString::number (settings->value (key).toDouble ());
		}
		return result;
	}

	std::shared_ptr<QSettings> CompParamsManager::GetSettings (QString compName) const
	{
		if (compName.isEmpty ())
			compName = CurrentInfo_.Name_;

		auto settings = new QSettings (QCoreApplication::organizationName (),
				QCoreApplication::applicationName () + "_Fenet");
		settings->beginGroup ("Compositors");
		settings->beginGroup (compName);

		return std::shared_ptr<QSettings> (settings,
				[] (QSettings *settings)
				{
					settings->endGroup ();
					settings->endGroup ();
					delete settings;
				});
	}

	void CompParamsManager::handleItemChanged (QStandardItem *item)
	{
		const auto& var = item->data (Role::Description);
		if (var.canConvert<Flag> ())
		{
			const auto& flag = var.value<Flag> ();
			const bool enabled = item->checkState () == Qt::Checked;

			ChangedFlags_ [CurrentInfo_.Name_] [flag.Name_] = enabled;
		}
		else if (var.canConvert<Param> ())
		{
			const auto& param = var.value<Param> ();
			const double value = item->data (Qt::EditRole).toDouble ();

			ChangedParams_ [CurrentInfo_.Name_] [param.Name_] = value;
		}
		else
			qWarning () << Q_FUNC_INFO
					<< "unknown item"
					<< var;
	}

	void CompParamsManager::save ()
	{
		QStringList keys = ChangedFlags_.keys () + ChangedParams_.keys ();
		keys.removeDuplicates ();
		if (keys.isEmpty ())
			return;

		bool changed = ChangedParams_.size ();

		for (const auto& key : keys)
		{
			auto settings = GetSettings (key);

			const auto& params = ChangedParams_ [key];
			for (const auto& name : params.keys ())
				settings->setValue (name, params [name]);

			const auto& oldFlags = settings->value ("__Flags").toStringList ().toSet ();
			auto newFlagsSet = oldFlags;

			const auto& flags = ChangedFlags_ [key];
			for (const auto& name : flags.keys ())
				if (flags [name])
					newFlagsSet << name;
				else
					newFlagsSet.remove (name);

			if (newFlagsSet != oldFlags)
			{
				settings->setValue ("__Flags", QStringList (newFlagsSet.toList ()));
				changed = true;
			}
		}

		if (changed)
			emit paramsChanged ();
	}

	void CompParamsManager::revert ()
	{
		ChangedFlags_.clear ();
		ChangedParams_.clear ();
	}
}
}
