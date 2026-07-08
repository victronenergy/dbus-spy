#ifndef ABSTRACT_OBJECT_LIST_MODEL_H
#define ABSTRACT_OBJECT_LIST_MODEL_H

#include <QAbstractListModel>

class VeQItem;

class AbstractObjectListModel: public QAbstractListModel
{
	Q_OBJECT
public:
	using QAbstractListModel::QAbstractListModel;

	virtual VeQItem *getItem(int index) const = 0;

	virtual QString getItemName(VeQItem *item) const = 0;

	virtual int indexOf(VeQItem *item) const = 0;

	// Index of the first item of the next/previous "group" after/before index, wrapping
	// around, or -1 if the model doesn't have a notion of groups. A group is relative to
	// index's own position in the tree -- the nearest enclosing point where there's more than
	// one real sub-item to jump between. See ObjectListModel for details; other models just
	// keep this default (no-op).
	virtual int nextGroup(int index) const { Q_UNUSED(index); return -1; }
	virtual int previousGroup(int index) const { Q_UNUSED(index); return -1; }

	// Same idea, but always at the outermost/top-level grouping for the whole list, regardless
	// of index's depth -- e.g. jumps straight from Settings/DigitalInput to Settings/DynamicEss
	// even from deep inside a large sub-collection like DynamicEss/Schedule/23/....
	virtual int nextSection(int index) const { Q_UNUSED(index); return -1; }
	virtual int previousSection(int index) const { Q_UNUSED(index); return -1; }
};

#endif // ABSTRACT_OBJECT_LIST_MODEL_H
