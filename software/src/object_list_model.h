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
	Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
public:
	ObjectListModel(VeQItem *root = 0, bool recursive = false, QObject *parent = 0);

	QString path() const;

	void setPath(const QString &path);

	bool recursive() const;

	void setRecursive(bool r);

	QString filterText() const;

	void setFilterText(const QString &t);

	virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

	virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;

	VeQItem *getItem(int index) const;

	VeQItem *getRoot() const;

	int indexOf(VeQItem *item) const;

signals:
	void pathChanged();

	void recursiveChanged();

	void filterTextChanged();

protected:
	virtual QHash<int, QByteArray> roleNames() const;

private slots:
	void onChildAdded(VeQItem *item);

	void onChildRemoved(VeQItem *item);

	void onItemStateChanged(VeQItem *item);

private:
	void startScan();

	void updateRoot();

	void updateFilteredList();

	void updateFilteredList(VeQItem *root, QList<VeQItem *> &items);

	QString mFilterText;
	VeQItem *mRoot;
	bool mRecursive;
	QList<VeQItem *> mFilteredObjects;
};

#endif // DBUSOBJECTMODEL_H
