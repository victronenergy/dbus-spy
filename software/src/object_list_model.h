#ifndef DBUSOBJECTMODEL_H
#define DBUSOBJECTMODEL_H

#include <QSet>
#include "abstract_object_list_model.h"

class ObjectListModel : public AbstractObjectListModel
{
	Q_OBJECT
	Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
	Q_PROPERTY(bool recursive READ recursive WRITE setRecursive NOTIFY recursiveChanged)
public:
	ObjectListModel(VeQItem *root = 0, bool recursive = false, QObject *parent = 0);

	QString path() const;

	void setPath(const QString &path);

	bool recursive() const;

	void setRecursive(bool r);

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

	void onItemStateChanged(VeQItem *item);

private:
	void startScan();

	void updateRoot();

	void addItems(VeQItem *item);

	bool tryInsertItem(VeQItem *item);

	void removeItem(VeQItem *item);

	void disconnectItems(VeQItem *item);

	VeQItem *mRoot = nullptr;
	bool mRecursive = false;
	QList<VeQItem *> mItems;
	QSet<VeQItem *> mConnectedItems;
};

#endif // DBUSOBJECTMODEL_H
