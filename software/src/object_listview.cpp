#include <QDBusVariant>
#include <velib/qt/ve_qitem.hpp>
#include "object_list_model.h"
#include "object_listview.h"
#include "list_view.h"

ObjectListView::ObjectListView(ObjectListModel *model, WINDOW *w,
							   QObject *parent):
	ListView(w, parent),
	mShowText(false)
{
	setModel(model);
}

QString ObjectListView::getValue(int index)
{
	VeQItem *item = getItem(index);
	if (item == 0)
		return QString();
	return item->getValue().toString();
}

void ObjectListView::setValue(int index, const QVariant &v)
{
	VeQItem *item = getItem(index);
	if (item == 0)
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

void ObjectListView::drawRow(int index, int width) const
{
	VeQItem *item = getItem(index);
	if (item == 0)
		return;
	VeQItem *root = static_cast<ObjectListModel *>(model())->getRoot();
	QString bind = item->getRelId(root);
	QString line;
	line.reserve(width);
	line = bind.mid(bind.indexOf('/') + 1);
	QString text;
	if (item->isLeaf()) {
		if (mShowText) {
			text = item->getText();
		} else {
			text = convertVariant(item->getValue());
		}
	}
	int gapSize = qMax(1, width - text.size() - line.size());
	for (int i=0; i<gapSize; ++i) {
		line.append(' ');
	}
	int textSize = width - line.size();
	for (int i=0; i<textSize; ++i) {
		line.append(text[i]);
	}
	waddstr(window(), line.toLatin1().data());
}

void ObjectListView::onValueChanged()
{
	if (!mShowText)
		updateItem();
}

void ObjectListView::onTextChanged()
{
	if (mShowText)
		updateItem();
}

void ObjectListView::onDestroyed()
{
	mItems.removeOne(static_cast<VeQItem *>(sender()));
}

void ObjectListView::updateItem()
{
	VeQItem *item = static_cast<VeQItem *>(sender());
	ObjectListModel *m = static_cast<ObjectListModel *>(model());
	int i = m->indexOf(item);
	redrawRows(i, i);
	wrefresh(window());
}

VeQItem *ObjectListView::getItem(int index) const
{
	ObjectListModel *m = static_cast<ObjectListModel *>(model());
	VeQItem *item = m->getItem(index);
	if (mItems.contains(item))
		return item;
	connect(item, SIGNAL(valueChanged(VeQItem *, QVariant)), this, SLOT(onValueChanged()));
	connect(item, SIGNAL(textChanged(VeQItem *, QString)), this, SLOT(onTextChanged()));
	connect(item, SIGNAL(destroyed()), this, SLOT(onDestroyed()));
	mItems.append(item);
	return item;
}

QString ObjectListView::convertVariant(const QVariant &value)
{
	// Workaround: the standard demarshall function does not convert QBusVariant objects to
	// QVariants. This causes problems in com.victronenergy.system/pvinverterids.
	if (value.canConvert<QDBusVariant>()) {
		QDBusVariant v = qvariant_cast<QDBusVariant>(value);
		return convertVariant(v.variant());
	}
	switch (value.type()) {
	case QVariant::Double:
		return QString::number(value.toDouble());
	case QVariant::String:
		return value.toString();
	case QVariant::Invalid:
		return "-";
	case QVariant::List:
	{
		QList<QVariant> list = value.toList();
		QString r = "[";
		foreach (QVariant v, list) {
			r.append(convertVariant(v));
			r.append(',');
		}
		if (r.size() > 1)
			r.chop(1);
		r.append("]");
		return r;
	}
	case QVariant::Map:
	{
		QMap<QString, QVariant> map = value.toMap();
		QString r = "{";
		foreach (QString key, map.keys()) {
			QVariant value = map.value(key);
			r.append(QString("'%1': %2,").arg(key).arg(convertVariant(value)));
		}
		if (r.size() > 1)
			r.chop(1);
		r.append("}");
		return r;
	}
	default:
		return QString::number(value.toInt());
	}
}
