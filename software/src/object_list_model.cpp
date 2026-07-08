#include <veutil/qt/ve_qitem.hpp>

#include "object_list_model.h"

ObjectListModel::ObjectListModel(VeQItem *root, bool recursive, bool showHistory, QObject *parent):
	AbstractObjectListModel(parent),
	mRoot(root),
	mRecursive(recursive),
	mShowHistory(showHistory)
{
	updateRoot();
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

// The ancestor of `item` at the nearest point, walking up from `item`, where the tree
// actually forks into more than one real sub-item -- i.e. the closest enclosing "collection".
// This is deliberately relative to `item` rather than fixed for the whole list: a service
// like Settings mostly forks once, right under its single "Settings" wrapper, into categories
// like DigitalInput/DynamicEss -- but a category can itself contain a real sub-collection
// (e.g. DynamicEss/Schedule/0.../54, each holding several leaf properties), and once the
// selection is inside one of those, jumping should work at *that* level, not haul all the way
// back out to the top category every time. So: keep climbing past a level as long as it isn't
// a genuine fork (0 or 1 branch children -- a wrapper or a uniform leaf-only node), and stop
// at the first level with more than one branch child.
//
// A level with unrelated flat leaf properties alongside its real branch children (e.g.
// com.victronenergy.system has Ac/Dc/... categories sitting next to loose scalars like
// Connected/Serial/ProductId right at the root) would otherwise give each of those leaves its
// own singleton group, degenerating Tab into one-item-at-a-time stepping through long runs of
// them. So when the found fork's child on our path is itself a leaf, every such leaf at that
// level collapses into one shared "everything else" group (identified by the fork itself)
// instead -- Tab always lands on a real (sub-)category or the start of a run of loose
// properties, never mid-run.
//
// Two items are in the same group iff this returns the same ancestor for both.
VeQItem *ObjectListModel::groupAncestor(VeQItem *item) const
{
	VeQItem *node = item;
	for (;;) {
		VeQItem *parent = node->itemParent();
		if (parent == nullptr)
			return node;
		int branchCount = 0;
		for (VeQItem *child: parent->itemChildren()) {
			if (!child->isLeaf())
				++branchCount;
		}
		if (branchCount > 1)
			return node->isLeaf() ? parent : node;
		if (parent == mRoot)
			return node;
		node = parent;
	}
}

int ObjectListModel::nextGroup(int index) const
{
	int n = mItems.size();
	if (index < 0 || index >= n)
		return -1;
	VeQItem *group = groupAncestor(mItems[index]);
	for (int step = 1; step <= n; ++step) {
		int i = (index + step) % n;
		if (groupAncestor(mItems[i]) != group)
			return i;
	}
	return -1; // only one group in the whole list
}

int ObjectListModel::previousGroup(int index) const
{
	int n = mItems.size();
	if (index < 0 || index >= n)
		return -1;
	VeQItem *group = groupAncestor(mItems[index]);
	// Step back past the current group to somewhere in the previous one.
	int i = index;
	for (int step = 1; step <= n; ++step) {
		i = (index - step + n) % n;
		if (groupAncestor(mItems[i]) != group)
			break;
		if (step == n)
			return -1; // only one group in the whole list
	}
	// Walk back further to the first item of that (previous) group.
	VeQItem *prevGroup = groupAncestor(mItems[i]);
	for (;;) {
		int j = (i - 1 + n) % n;
		if (j == index || groupAncestor(mItems[j]) != prevGroup)
			break;
		i = j;
	}
	return i;
}

// Like groupAncestor(), but fixed for the whole list instead of relative to `item`: always the
// outermost fork, found by descending from mRoot through single-branch wrapper chains (same
// leaf-lumping rule as groupAncestor() once there). Used for nextSection()/previousSection(),
// which -- unlike nextGroup()/previousGroup() -- always jump between top-level sections
// (Settings/DigitalInput to Settings/DynamicEss) no matter how deep `index` currently is.
VeQItem *ObjectListModel::sectionAncestor(VeQItem *item) const
{
	VeQItem *effectiveRoot = mRoot;
	for (;;) {
		VeQItem *onlyBranch = nullptr;
		int branchCount = 0;
		for (VeQItem *child: effectiveRoot->itemChildren()) {
			if (!child->isLeaf()) {
				++branchCount;
				onlyBranch = child;
			}
		}
		if (branchCount != 1)
			break;
		effectiveRoot = onlyBranch;
	}
	VeQItem *node = item;
	while (node != nullptr && node->itemParent() != effectiveRoot)
		node = node->itemParent();
	if (node == nullptr)
		return item;
	return node->isLeaf() ? effectiveRoot : node;
}

int ObjectListModel::nextSection(int index) const
{
	int n = mItems.size();
	if (index < 0 || index >= n)
		return -1;
	VeQItem *section = sectionAncestor(mItems[index]);
	for (int step = 1; step <= n; ++step) {
		int i = (index + step) % n;
		if (sectionAncestor(mItems[i]) != section)
			return i;
	}
	return -1; // only one section in the whole list
}

int ObjectListModel::previousSection(int index) const
{
	int n = mItems.size();
	if (index < 0 || index >= n)
		return -1;
	VeQItem *section = sectionAncestor(mItems[index]);
	// Step back past the current section to somewhere in the previous one.
	int i = index;
	for (int step = 1; step <= n; ++step) {
		i = (index - step + n) % n;
		if (sectionAncestor(mItems[i]) != section)
			break;
		if (step == n)
			return -1; // only one section in the whole list
	}
	// Walk back further to the first item of that (previous) section.
	VeQItem *prevSection = sectionAncestor(mItems[i]);
	for (;;) {
		int j = (i - 1 + n) % n;
		if (j == index || sectionAncestor(mItems[j]) != prevSection)
			break;
		i = j;
	}
	return i;
}

void ObjectListModel::onChildAdded(VeQItem *item)
{
	if (mRecursive || item->itemParent() == mRoot)
		addItems(item);
}

void ObjectListModel::onChildRemoved(VeQItem *item)
{
	removeItem(item);
	disconnectItems(item);
}

void ObjectListModel::onItemStateChanged()
{
	VeQItem *item = static_cast<VeQItem *>(sender());

	switch (item->getState()) {
	case VeQItem::Idle:
		item->getValue();
		break;
	case VeQItem::Synchronized:
		addItems(item);
		break;
	case VeQItem::Requested:
		// Do nothing
		break;
	case VeQItem::Offline:
		removeItem(item);
		break;
	default:
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
	if (!mShowHistory && item->getRelId(mRoot).startsWith("/History"))
		return;
	if (mItems.contains(item))
		return;
	bool connected = mConnectedItems.contains(item);
	if (!connected) {
		mConnectedItems.insert(item);
		if (item->getState() == VeQItem::Idle)
			item->getValue();
		connect(item, SIGNAL(stateChanged(VeQItem::State)), this, SLOT(onItemStateChanged()));
	}
	tryInsertItem(item);
	if (item->isLeaf()) {
		Q_ASSERT(item->itemChildren().isEmpty());
		return;
	}
	if (!connected) {
		connect(item, SIGNAL(childAdded(VeQItem*)), this, SLOT(onChildAdded(VeQItem*)));
		connect(item, SIGNAL(childAboutToBeRemoved(VeQItem*)), this, SLOT(onChildRemoved(VeQItem*)));
	}
	if (item != mRoot && !mRecursive)
		return;
	for (VeQItem *child: item->itemChildren())
		addItems(child);
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
	int r = std::lower_bound(mItems.begin(), mItems.end(), item,
		[](VeQItem *i0, VeQItem *i1) { return i0->uniqueId() < i1->uniqueId(); }) - mItems.begin();
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
	mConnectedItems.remove(item);
	disconnect(item, SIGNAL(stateChanged(VeQItem::State)), this, SLOT(onItemStateChanged()));
	if (item->isLeaf()) {
		Q_ASSERT(item->itemChildren().isEmpty());
		return;
	}
	disconnect(item, SIGNAL(childAdded(VeQItem*)), this, SLOT(onChildAdded(VeQItem*)));
	disconnect(item, SIGNAL(childAboutToBeRemoved(VeQItem*)), this, SLOT(onChildRemoved(VeQItem*)));
	if (item != mRoot && !mRecursive)
		return;
	for (VeQItem *child: item->itemChildren())
		disconnectItems(child);
}
