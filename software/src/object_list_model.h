#ifndef DBUSOBJECTMODEL_H
#define DBUSOBJECTMODEL_H

#include <QAbstractListModel>
#include <QByteArray>
#include <QList>

class ServiceObserver;
class VeQItem;

class ObjectListModel : public QAbstractListModel
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

	virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

	virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;

	VeQItem *getItem(int index) const;

	VeQItem *getRoot() const;

	int indexOf(VeQItem *item) const;

signals:
	void pathChanged();

	void recursiveChanged();

protected:
	virtual QHash<int, QByteArray> roleNames() const;

private slots:
	void onChildAdded(VeQItem *item);

	void onChildRemoved(VeQItem *item);

	void onItemStateChanged(VeQItem *item);

private:
	void startScan();

	void updateRoot();

	void addItems(VeQItem *item);

	void insertItem(VeQItem *item);

	void removeItem(VeQItem *item);

	void disconnectItems(VeQItem *item);

	VeQItem *mRoot;
	bool mRecursive;
	QList<VeQItem *> mItems;
};

#endif // DBUSOBJECTMODEL_H
