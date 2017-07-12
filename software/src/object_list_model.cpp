#include <velib/qt/ve_qitem.hpp>
#include "object_list_model.h"

ObjectListModel::ObjectListModel(VeQItem *root, bool recursive, QObject *parent):
	AbstractObjectListModel(parent),
	mRoot(root),
	mRecursive(recursive)
{
	updateRoot();
}

QString ObjectListModel::path() const
{
	return mRoot == nullptr ? QString{} : mRoot->uniqueId();
}

void ObjectListModel::setPath(const QString &path)
{
	QString oldPath = mRoot == nullptr ? QString{} : mRoot->uniqueId();
	if (oldPath == path)
		return;
	mRoot = VeQItems::getRoot()->itemGet(path);
	updateRoot();
	emit pathChanged();
}

bool ObjectListModel::recursive() const
{
	return mRecursive;
}

void ObjectListModel::setRecursive(bool r)
{
	if (mRecursive == r)
		return;
	mRecursive = r;
	updateRoot();
	emit recursiveChanged();
}

QVariant ObjectListModel::data(const QModelIndex &index, int role) const
{
	Q_UNUSED(role)
	if (!index.isValid())
		return QVariant();
	int r = index.row();
	if (r < 0 || r > mItems.size())
		return QVariant();
	return mItems[r]->uniqueId();
}

int ObjectListModel::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	Q_ASSERT(!parent.isValid());
	return mItems.size();
}

VeQItem *ObjectListModel::getItem(int index) const
{
	if (index < 0 || index >= mItems.size())
		return 0;
	return mItems[index];
}

QString ObjectListModel::getItemName(VeQItem *item) const
{
	QString name = item->getRelId(mRoot);
	if (name.startsWith('/'))
		name.remove(0, 1);
	return name;
}

int ObjectListModel::indexOf(VeQItem *item) const
{
	return mItems.indexOf(item);
}

void ObjectListModel::onChildAdded(VeQItem *item)
{
	if (mRecursive || item->itemParent() == mRoot)
		addItems(item);
}

void ObjectListModel::onChildRemoved(VeQItem *item)
{
	removeItem(item);
}

void ObjectListModel::onItemStateChanged(VeQItem *item)
{
	if (item->getState() == VeQItem::Requested)
		return;
	if (item->getState() == VeQItem::Offline)
		removeItem(item);
	else if (item->getState() != VeQItem::Requested)
		insertItem(item);
}

void ObjectListModel::updateRoot()
{
	disconnectItems(mRoot);
	mItems.clear();
	addItems(mRoot);
}

void ObjectListModel::addItems(VeQItem *item)
{
	if (item == nullptr)
		return;
	Q_ASSERT(!mItems.contains(item));
	connect(item, SIGNAL(stateChanged(VeQItem *, State)),
			this, SLOT(onItemStateChanged(VeQItem *)));
	if (item->getState() != VeQItem::Offline)
		insertItem(item);
	if (item->isLeaf()) {
		Q_ASSERT(item->itemChildren().isEmpty());
		return;
	}
	connect(item, SIGNAL(childAdded(VeQItem *)),
			this, SLOT(onChildAdded(VeQItem *)));
	connect(item, SIGNAL(childAboutToBeRemoved(VeQItem *)),
			this, SLOT(onChildRemoved(VeQItem *)));
	if (item != mRoot && !mRecursive)
		return;
	for (int i = 0; ; ++i) {
		VeQItem *child = item->itemChild(i);
		if (child == 0)
			break;
		addItems(child);
	}
}

void ObjectListModel::insertItem(VeQItem *item)
{
	if (item == nullptr)
		return;
	if (!item->isLeaf() && mRecursive)
		return;
	if (item == mRoot)
		return;
	if (mItems.contains(item))
		return;
	QString uid = item->uniqueId();
	int r = 0;
	for (;r<mItems.size(); ++r) {
		if (mItems[r]->uniqueId() > uid)
			break;
	}
	beginInsertRows(QModelIndex(), r, r);
	mItems.insert(r, item);
	endInsertRows();
}

void ObjectListModel::removeItem(VeQItem *item)
{
	int i = mItems.indexOf(item);
	if (i != -1) {
		beginRemoveRows(QModelIndex(), i, i);
		mItems.removeAt(i);
		endRemoveRows();
	}
}

void ObjectListModel::disconnectItems(VeQItem *item)
{
	if (item == nullptr)
		return;
	disconnect(item, SIGNAL(stateChanged(VeQItem *, State)),
			   this, SLOT(onItemStateChanged(VeQItem *)));
	if (item->isLeaf()) {
		Q_ASSERT(item->itemChildren().isEmpty());
		return;
	}
	disconnect(item, SIGNAL(childAdded(VeQItem *)),
			   this, SLOT(onChildAdded(VeQItem *)));
	disconnect(item, SIGNAL(childAboutToBeRemoved(VeQItem *)),
			   this, SLOT(onChildRemoved(VeQItem *)));
	if (item != mRoot && !mRecursive)
		return;
	for (int i=0;;++i) {
		VeQItem *child = item->itemChild(i);
		if (child == nullptr)
			break;
		disconnectItems(child);
	}
}
