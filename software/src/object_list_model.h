#ifndef DBUSOBJECTMODEL_H
#define DBUSOBJECTMODEL_H

#include <QSet>
#include "abstract_object_list_model.h"

class ObjectListModel : public AbstractObjectListModel
{
	Q_OBJECT

public:
	ObjectListModel(VeQItem *root = 0, bool recursive = false, bool showHistory = false,
					QObject *parent = 0);

	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	VeQItem *getItem(int index) const override;
	QString getItemName(VeQItem *item) const override;
	int indexOf(VeQItem *item) const override;

signals:
	void pathChanged();
	void recursiveChanged();

private slots:
	void onChildAdded(VeQItem *item);
	void onChildRemoved(VeQItem *item);
	void onItemStateChanged();

private:
	void updateRoot();
	void addItems(VeQItem *item);
	bool tryInsertItem(VeQItem *item);
	void removeItem(VeQItem *item);
	void disconnectItems(VeQItem *item);

	VeQItem *mRoot = nullptr;
	bool mRecursive = false;
	bool mShowHistory = false;
	QList<VeQItem *> mItems;
	QSet<VeQItem *> mConnectedItems;
};

#endif
