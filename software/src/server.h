#ifndef SERVER_H
#define SERVER_H

#include <QByteArray>
#include <QHash>
#include <QObject>

class FavoritesListModel;
class QModelIndex;
class VeQItem;

class Server : public QObject
{
	Q_OBJECT
public:
	// Forks the privilege-dropped server child (see server_child.h) and
	// prints the outcome to stdout. Returns nullptr on any failure -- the
	// caller is expected to continue running without the data feed, most
	// commonly because another dbus-spy instance already owns the port.
	static Server *start(int port, QObject *parent = nullptr);

	~Server() override;

	void attach(FavoritesListModel *favorites);

private slots:
	void onFavoritesRowsInserted(const QModelIndex &parent, int first, int last);
	void onFavoritesRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
	void onValueChanged();
	void onItemDestroyed();

private:
	Server(int fd, QObject *parent);

	void trackItem(VeQItem *item);
	void sendSample(VeQItem *item);
	void sendRemove(const QByteArray &path);

	int mFd = -1;
	FavoritesListModel *mFavorites = nullptr;
	// Value is the item's uniqueId at the time it was tracked -- needed to
	// tell the child which series to drop, since by the time onItemDestroyed()
	// runs the VeQItem itself may already be torn down.
	QHash<VeQItem *, QByteArray> mTracked;
};

#endif
