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
};

#endif // ABSTRACT_OBJECT_LIST_MODEL_H
