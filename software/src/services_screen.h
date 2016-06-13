#ifndef SERVICESSCREEN_H
#define SERVICESSCREEN_H

#include <QObject>
#include <QStringList>
#include <ncurses.h>

class ListView;
class VeQItem;

class ServicesScreen : public QObject
{
	Q_OBJECT
public:
	ServicesScreen(VeQItem *root, QObject *parent = 0);

	~ServicesScreen();

	virtual bool handleInput(int c);

signals:
	void serviceSelected(VeQItem *serviceRoot);

private:
	WINDOW *mTitleWindow;
	WINDOW *mListViewWindow;
	ListView *mListView;
};

#endif // SERVICESSCREEN_H
