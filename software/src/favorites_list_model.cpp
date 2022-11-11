#include <QSettings>
#include <velib/qt/ve_qitem.hpp>
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
	connect(item, SIGNAL(stateChanged(VeQItem*, State)),
			this, SLOT(onItemStateChanged()));
}

void FavoritesListModel::disconnectItem(VeQItem *item)
{
	disconnect(item, SIGNAL(stateChanged(VeQItem*,State)),
			   this, SLOT(onItemStateChanged()));
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
