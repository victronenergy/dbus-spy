#include <velib/qt/ve_qitem.hpp>
#include "object_list_model.h"

ObjectListModel::ObjectListModel(VeQItem *root, bool recursive, QObject *parent):
	QAbstractListModel(parent),
	mRoot(root),
	mRecursive(recursive)
{
	updateRoot();
}

QString ObjectListModel::path() const
{
	return mRoot == 0 ? QString() : mRoot->uniqueId();
}

void ObjectListModel::setPath(const QString &path)
{
	QString oldPath = mRoot == 0 ? QString() : mRoot->uniqueId();
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
	updateFilteredList();
	emit recursiveChanged();
}

QString ObjectListModel::filterText() const
{
	return mFilterText;
}

void ObjectListModel::setFilterText(const QString &t)
{
	if (mFilterText == t)
		return;
	mFilterText = t;
	emit filterTextChanged();
	updateFilteredList();
}

QVariant ObjectListModel::data(const QModelIndex &index, int role) const
{
	Q_UNUSED(role)
	if (!index.isValid())
		return QVariant();
	int r = index.row();
	if (r < 0 || r > mFilteredObjects.size())
		return QVariant();
	return mFilteredObjects[r]->uniqueId();
}

int ObjectListModel::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	Q_ASSERT(!parent.isValid());
	return mFilteredObjects.size();
}

VeQItem *ObjectListModel::getItem(int index) const
{
	if (index < 0 || index >= mFilteredObjects.size())
		return 0;
	return mFilteredObjects[index];
}

VeQItem *ObjectListModel::getRoot() const
{
	return mRoot;
}

int ObjectListModel::indexOf(VeQItem *item) const
{
	return mFilteredObjects.indexOf(item);
}

QHash<int, QByteArray> ObjectListModel::roleNames() const
{
	QHash<int, QByteArray> result;
	result[Qt::UserRole + 1] = "name";
	return result;
}

void ObjectListModel::onChildAdded(VeQItem *item)
{
	Q_UNUSED(item)
	updateFilteredList();
}

void ObjectListModel::onChildRemoved(VeQItem *item)
{
	Q_UNUSED(item)
	updateFilteredList();
}

void ObjectListModel::updateRoot()
{
	updateFilteredList();
	if (mRoot != 0) {
		connect(mRoot, SIGNAL(childAdded(VeQItem *)),
				this, SLOT(onChildAdded(VeQItem *)));
		connect(mRoot, SIGNAL(childAboutToBeRemoved(VeQItem *)),
				this, SLOT(onChildRemoved(VeQItem *)));
	}
}

void ObjectListModel::updateFilteredList()
{
	if (!mFilteredObjects.isEmpty()) {
		beginRemoveRows(QModelIndex(), 0, mFilteredObjects.size() - 1);
		mFilteredObjects.clear();
		endRemoveRows();
	}
	QList<VeQItem *> items;
	if (mRoot != 0)
		updateFilteredList(mRoot, items);
	if (!items.isEmpty()) {
		beginInsertRows(QModelIndex(), 0, items.size() - 1);
		mFilteredObjects = items;
		endInsertRows();
	}
}

void ObjectListModel::updateFilteredList(VeQItem *root, QList<VeQItem *> &items)
{
	for(int i=0;;++i) {
		VeQItem *item = root->itemChild(i);
		if (item == 0)
			break;
		if (!mRecursive || item->isLeaf()) {
			if (mFilterText.isEmpty() || item->uniqueId().contains(mFilterText))
				items.append(item);
		} else {
			updateFilteredList(item, items);
		}
	}
}
