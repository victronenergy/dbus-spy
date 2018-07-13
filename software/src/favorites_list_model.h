#ifndef FAVORITES_LIST_MODEL_H
#define FAVORITES_LIST_MODEL_H

#include "abstract_object_list_model.h"
#include <QList>
#include <QStringList>

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

private slots:
	void onServiceAdded(VeQItem *item);

	void onItemStateChanged(VeQItem *item);

private:
	void adjustSettings();

	void connectItem(VeQItem *item);

	void disconnectItem(VeQItem *item);

	VeQItem *getServiceRoot(VeQItem *item) const;

	QList<VeQItem *> mItems;
	// Workaround for a problem in VeQItems: if an item pointing to an object within a D-Bus service
	// is created (using itemGetOrCreate), the function VeQItemDBus::attachRootItem will be called,
	// when the VeQItem representing the service is created. This seems to have no effect.
	// This does not have the desired effect if the D-Bus service does not exist yet. If the service
	// appears later on, attachRootItem is not called again. This means that the objects within the
	// service will not be retrieved using a GetValue of the service root. So, the service itself is
	// present in the tree, but most of its child objects are missing.
	// This problem may occur when starting the application with a number of stored favorites.
	// So before creating a VeQItem from a stored path, we check if the corresponding D-Bus service
	// exists. If not, the path is stored in mPendingPaths and the item will be created after the
	// D-Bus service has appeared.
	QStringList mPendingPaths;
	VeQItem *mRoot = nullptr;
	QSettings *mSettings = nullptr;
};

#endif // FAVORITES_LIST_MODEL_H
