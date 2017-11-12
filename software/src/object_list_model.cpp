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
	switch (item->getState()) {
	case VeQItem::Requested:
		// Do nothing
		break;
	case VeQItem::Offline:
		removeItem(item);
		break;
	default:
		tryInsertItem(item);
		break;
	}
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
	tryInsertItem(item);
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

bool ObjectListModel::tryInsertItem(VeQItem *item)
{
	if (item == nullptr)
		return false;
	if (!item->isLeaf() && mRecursive)
		return false;
	if (item == mRoot)
		return false;
	if (item->getState() == VeQItem::Offline || item->getState() == VeQItem::Requested)
		return false;
	if (mItems.contains(item))
		return false;
	QString uid = item->uniqueId();
	int r = 0;
	for (;r<mItems.size(); ++r) {
		if (mItems[r]->uniqueId() > uid)
			break;
	}
	beginInsertRows(QModelIndex(), r, r);
	mItems.insert(r, item);
	endInsertRows();
	return true;
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
