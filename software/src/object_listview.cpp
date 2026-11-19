#include <veutil/qt/ve_qitem.hpp>

#include "favorites_list_model.h"
#include "object_list_model.h"
#include "object_listview.h"
#include "list_view.h"
#include "value_format.h"

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
			text = mShowText ? item->getText() : formatVariant(item->getValue());
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
