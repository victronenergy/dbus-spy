#ifndef SERVICESSCREEN_H
#define SERVICESSCREEN_H

#include <QObject>
#include <QStringList>
#include <cursesw.h>

class AbstractObjectListModel;
class ObjectListView;
class VeQItem;

class ServicesScreen : public QObject
{
	Q_OBJECT
public:
	ServicesScreen(VeQItem *root, QObject *parent = 0);
	~ServicesScreen();

	virtual bool handleInput(wint_t c);
	void repaint();

	AbstractObjectListModel* getModel() const { return mModel; }
	ObjectListView* getListView() const { return mListView; }

signals:
	void serviceSelected(VeQItem *serviceRoot);

private:
	WINDOW *mTitleWindow = nullptr;
	WINDOW *mListViewWindow = nullptr;
	ObjectListView *mListView = nullptr;
	AbstractObjectListModel *mModel = nullptr;
};

#endif
