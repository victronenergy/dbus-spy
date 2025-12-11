#include <QDBusVariant>

#include <veutil/qt/ve_qitem.hpp>

#include "favorites_list_model.h"
#include "object_list_model.h"
#include "object_listview.h"
#include "list_view.h"

ObjectListView::ObjectListView(AbstractObjectListModel *model, WINDOW *w, QObject *parent):
	ListView(w, parent)
{
	setModel(model);
}

QString ObjectListView::getValue(int index)
{
	VeQItem *item = getItem(index);
	if (item == nullptr)
		return QString{};
	return item->getValue().toString();
}

void ObjectListView::setValue(int index, const QVariant &v)
{
	VeQItem *item = getItem(index);
	if (item == nullptr)
		return;
	item->setValue(v);
}

bool ObjectListView::showText() const
{
	return mShowText;
}

void ObjectListView::setShowText(bool s)
{
	if (mShowText == s)
		return;
	mShowText = s;
	redraw();
	wrefresh(window());
}

void ObjectListView::setFavorites(FavoritesListModel *favorites)
{
	mFavorites = favorites;
}

void ObjectListView::drawRow(int index, int width) const
{
	VeQItem *item = getItem(index);
	if (item == 0)
		return;
	QString line;
	line.reserve(width);
	line = model()->getItemName(item);
	QString text;
	if (item->isLeaf()) {
		if (item->getState() != VeQItem::Offline)
			text = mShowText ? item->getText() : convertVariant(item->getValue());
	} else {
		VeQItem *productName = item->itemGetOrCreate("ProductName");
		VeQItem *customName = item->itemGetOrCreate("CustomName");
		registerItem(productName);
		registerItem(customName);
		QString customNameStr = customName->getValue().toString();
		if (customNameStr != "")
			text = customNameStr;
		else
			text = productName->getValue().toString();
	}
	int gapSize = qMax(1, width - text.size() - line.size());
	for (int i=0; i<gapSize; ++i)
		line.append(' ');
	int textSize = width - line.size();
	for (int i=0; i<textSize; ++i)
		line.append(text[i]);
	waddstr(window(), line.toUtf8().data());
}

bool ObjectListView::isEmphasized(int index) const
{
	if (mFavorites == nullptr)
		return false;
	VeQItem *item = getItem(index);
	if (item == nullptr)
		return false;
	if (model() == mFavorites)
		return mFavorites->isServiceRoot(item);
	return mFavorites->hasItem(item);
}

void ObjectListView::onValueChanged()
{
	if (!mShowText)
		updateItem(static_cast<VeQItem *>(sender()));
}

void ObjectListView::onTextChanged()
{
	if (mShowText)
		updateItem(static_cast<VeQItem *>(sender()));
}

void ObjectListView::onStateChanged()
{
	updateItem(static_cast<VeQItem *>(sender()));
}

void ObjectListView::onDestroyed()
{
	mItems.removeOne(static_cast<VeQItem *>(sender()));
}

void ObjectListView::updateItem(VeQItem *item)
{
	int i = model()->indexOf(item);
	if (i == -1) {
		// Check if this is a ProductName/CustomName item representing its service.
		VeQItem *parent = item->itemParent();
		if (parent != nullptr)
			i = model()->indexOf(parent);
	}
	if (i == -1)
		return;
	redrawRows(i, i);
	wrefresh(window());
}

VeQItem *ObjectListView::getItem(int index) const
{
	AbstractObjectListModel *m = model();
	VeQItem *item = m->getItem(index);
	registerItem(item);
	return item;
}

void ObjectListView::registerItem(VeQItem *item) const
{
	if (mItems.contains(item))
		return;
	connect(item, SIGNAL(valueChanged(QVariant)), this, SLOT(onValueChanged()));
	connect(item, SIGNAL(textChanged(QString)), this, SLOT(onTextChanged()));
	connect(item, SIGNAL(stateChanged(VeQItem::State)), this, SLOT(onStateChanged()));
	connect(item, SIGNAL(destroyed()), this, SLOT(onDestroyed()));
	mItems.append(item);
}

QString ObjectListView::convertVariant(const QVariant &value)
{
	// Workaround: the standard demarshall function does not convert QBusVariant objects to
	// QVariants. This causes problems in com.victronenergy.system/pvinverterids.
	if (value.canConvert<QDBusVariant>())
	{
		QDBusVariant v = qvariant_cast<QDBusVariant>(value);
		return convertVariant(v.variant());
	}
	switch (value.typeId()) {
	case QMetaType::UChar:
		return QString::number(value.toUInt());
	case QMetaType::Double:
		return QString::number(value.toDouble());
	case QMetaType::UnknownType:
		return "-";
	case QMetaType::QStringList:
		return "[" + value.toStringList().join(",") + "]";
	case QMetaType::QVariantList:
	{
		QList<QVariant> list = value.toList();
		QString r = "[";
		for (QVariant &v: list)
		{
			r.append(convertVariant(v));
			r.append(',');
		}
		if (r.size() > 1)
			r.chop(1);
		r.append("]");
		return r;
	}
	case QMetaType::QVariantMap:
	{
		QMap<QString, QVariant> map = value.toMap();
		QString r = "{";
		for (auto it = map.begin(); it != map.end(); ++it)
			r.append(QString("'%1': %2,").arg(it.key(), convertVariant(it.value())));
		if (r.size() > 1)
			r.chop(1);
		r.append("}");
		return r;
	}
	default:
		return value.toString();
	}
}
