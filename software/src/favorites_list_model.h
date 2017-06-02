#ifndef FAVORITES_LIST_MODEL_H
#define FAVORITES_LIST_MODEL_H

#include "abstract_object_list_model.h"
#include <QList>

class QSettings;
class VeQItem;

class FavoritesListModel : public AbstractObjectListModel
{
	Q_OBJECT
public:
	FavoritesListModel(VeQItem *root, QObject *parent = 0);

	VeQItem *getItem(int index) const override;

	QString getItemName(VeQItem *item) const override;

	int indexOf(VeQItem *item) const override;

	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;

	void addItem(VeQItem *item);

	void removeItem(VeQItem *item);

	bool hasItem(VeQItem *item) const;

	bool isServiceRoot(VeQItem *item) const;

private:
	void adjustSettings();

	VeQItem *getServiceRoot(VeQItem *item) const;

	QList<VeQItem *> mItems;
	VeQItem *mRoot = nullptr;
	QSettings *mSettings = nullptr;
};

#endif // FAVORITES_LIST_MODEL_H
