#include <QSettings>

#include <veutil/qt/ve_qitem.hpp>

#include "favorites_list_model.h"

FavoritesListModel::FavoritesListModel(VeQItem *root, QObject *parent):
	AbstractObjectListModel(parent),
	mRoot(root),
	mSettings(new QSettings("ejv", "dbus-spy", this))
{
	int size = mSettings->beginReadArray("favorites");
	for (int index = 0; index < size; ++index) {
		mSettings->setArrayIndex(index);
		QString path = mSettings->value("path").toString();
		// Workaround: do not create items if the underlying D-Bus service is not present (yet).
		QString serviceName = path.section('/', 1, 1);
		VeQItem *service = root->itemGet(serviceName);
		if (service == nullptr) {
			mPendingPaths.append(path);
			continue;
		}

		VeQItem *item = root->itemGetOrCreate(path);
		VeQItem *itemRoot = getServiceRoot(item);
		int i = mItems.indexOf(itemRoot);
		if (i == -1) {
			mItems.append(itemRoot);
			mItems.append(item);
		} else {
			++i;
			mItems.insert(i, item);
		}
		connectItem(item);
	}
	mSettings->endArray();
	connect(root, SIGNAL(childAdded(VeQItem*)), this, SLOT(onServiceAdded(VeQItem*)));
}

VeQItem *FavoritesListModel::getItem(int index) const
{
	if (index < 0 || index >= mItems.size())
		return 0;
	return mItems[index];
}

QString FavoritesListModel::getItemName(VeQItem *item) const
{
	VeQItem *itemRoot = getServiceRoot(item);
	if (itemRoot == nullptr)
		return QString{};
	return isServiceRoot(item) ? item->id() : item->getRelId(itemRoot);
}

int FavoritesListModel::indexOf(VeQItem *item) const
{
	return mItems.indexOf(item);
}

QVariant FavoritesListModel::data(const QModelIndex &index, int role) const
{
	Q_UNUSED(role)
	if (!index.isValid())
		return QVariant();
	int r = index.row();
	if (r < 0 || r > mItems.size())
		return QVariant();
	return mItems[r]->uniqueId();
}

int FavoritesListModel::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	Q_ASSERT(!parent.isValid());
	return mItems.size();
}

void FavoritesListModel::addItem(VeQItem *item)
{
	if (item == nullptr)
		return;
	if (mItems.contains(item))
		return;
	VeQItem *itemRoot = getServiceRoot(item);
	int i = mItems.indexOf(itemRoot);
	if (i == -1) {
		int r = mItems.size();
		beginInsertRows(QModelIndex(), r, r + 1);
		mItems.append(itemRoot);
		mItems.append(item);
		endInsertRows();
	} else {
		++i;
		beginInsertRows(QModelIndex(), i, i);
		mItems.insert(i, item);
		endInsertRows();
	}
	connectItem(item);
	adjustSettings();
}

void FavoritesListModel::removeItem(VeQItem *item)
{
	if (item == nullptr)
		return;
	int i = mItems.indexOf(item);
	if (i == -1)
		return;
	int j = i + 1;
	if (isServiceRoot(item)) {
		for (; j<mItems.size(); ++j) {
			VeQItem *it = mItems[j];
			if (getServiceRoot(it) != item)
				break;
		}
	}
	beginRemoveRows(QModelIndex(), i , j - 1);
	for (;j > i; --j)
		mItems.removeAt(i);
	endRemoveRows();
	disconnectItem(item);
	adjustSettings();
}

bool FavoritesListModel::hasItem(VeQItem *item) const
{
	return mItems.contains(item);
}

bool FavoritesListModel::isServiceRoot(VeQItem *item) const
{
	return item->itemParent() == mRoot;
}

bool FavoritesListModel::moveUp(VeQItem *item)
{
	return moveItem(item, -1);
}

bool FavoritesListModel::moveDown(VeQItem *item)
{
	return moveItem(item, 1);
}

void FavoritesListModel::onServiceAdded(VeQItem *item)
{
	QString serviceName = item->id();
	for (auto it = mPendingPaths.begin(); it != mPendingPaths.end();) {
		if (it->section('/', 1, 1) == serviceName) {
			VeQItem *item = mRoot->itemGetOrCreate(*it);
			addItem(item);
			it = mPendingPaths.erase(it);
		} else {
			++it;
		}
	}
}

void FavoritesListModel::onItemStateChanged()
{
	VeQItem *item = static_cast<VeQItem *>(sender());

	if (item->getState() == VeQItem::Idle) {
		// We get here when a D-Bus service is restarted. When the service is stopped state changes
		// to Offline, when it is created again, state changes to Idle.
		item->getValue();
	}
}

void FavoritesListModel::adjustSettings()
{
	int count = 0;
	for (VeQItem *item: mItems) {
		if (!isServiceRoot(item))
			++count;
	}
	mSettings->remove("favorites");
	mSettings->beginWriteArray("favorites", count);
	int index = 0;
	for (VeQItem *item: mItems) {
		if (!isServiceRoot(item)) {
			mSettings->setArrayIndex(index++);
			mSettings->setValue("path", item->getRelId(mRoot));
		}
	}
	mSettings->endArray();
}

void FavoritesListModel::connectItem(VeQItem *item)
{
	if (item->getState() == VeQItem::Idle)
		item->getValue();
	connect(item, SIGNAL(stateChanged(VeQItem::State)), this, SLOT(onItemStateChanged()));
}

void FavoritesListModel::disconnectItem(VeQItem *item)
{
	disconnect(item, SIGNAL(stateChanged(VeQItem::State)), this, SLOT(onItemStateChanged()));
}

VeQItem *FavoritesListModel::getServiceRoot(VeQItem *item) const
{
	VeQItem *prev = nullptr;
	for (; item != nullptr; item = item->itemParent()) {
		if (item == mRoot)
			return prev;
		prev = item;
	}
	return nullptr;
}

int FavoritesListModel::groupStart(VeQItem *serviceRoot) const
{
	return mItems.indexOf(serviceRoot);
}

int FavoritesListModel::groupEnd(int start) const
{
	for (int i = start + 1; i < mItems.size(); ++i) {
		if (isServiceRoot(mItems[i]))
			return i;
	}
	return mItems.size();
}

bool FavoritesListModel::moveItem(VeQItem *item, int offset)
{
	if (item == nullptr)
		return false;
	int i = mItems.indexOf(item);
	if (i == -1)
		return false;

	if (isServiceRoot(item)) {
		// Move the whole group (service root + its favorite items) up or down, swapping it
		// with the adjacent group.
		int start = i;
		int end = groupEnd(start);
		int otherStart;
		int otherEnd;
		if (offset < 0) {
			otherEnd = start;
			otherStart = -1;
			for (int j = start - 1; j >= 0; --j) {
				if (isServiceRoot(mItems[j])) {
					otherStart = j;
					break;
				}
			}
			if (otherStart == -1)
				return false;
		} else {
			if (end >= mItems.size())
				return false;
			otherStart = end;
			otherEnd = groupEnd(otherStart);
		}
		QList<VeQItem *> group = mItems.mid(start, end - start);
		QList<VeQItem *> other = mItems.mid(otherStart, otherEnd - otherStart);
		// group and other are adjacent (otherStart == end or otherEnd == start), so together
		// they form one contiguous range: remove it as a whole in one reverse pass (removing
		// group and other via two separately-indexed passes is wrong -- removing the first
		// range shifts the indices the second pass still relies on) and reinsert it reordered.
		int lo = qMin(start, otherStart);
		int hi = qMax(end, otherEnd);
		for (int j = hi - 1; j >= lo; --j)
			mItems.removeAt(j);
		QList<VeQItem *> merged = offset < 0 ? group + other : other + group;
		for (int j = 0; j < merged.size(); ++j)
			mItems.insert(lo + j, merged[j]);
		emit dataChanged(index(lo), index(lo + merged.size() - 1));
	} else {
		VeQItem *serviceRoot = getServiceRoot(item);
		int start = groupStart(serviceRoot);
		int end = groupEnd(start);
		int j = i + offset;
		// The first entry of a group is the service root itself, so favorite items can only
		// be reordered among the other entries of their own group.
		if (j <= start || j >= end)
			return false;
		mItems[i] = mItems[j];
		mItems[j] = item;
		emit dataChanged(index(qMin(i, j)), index(qMax(i, j)));
	}
	adjustSettings();
	emit orderChanged();
	return true;
}
