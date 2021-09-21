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
	void repaint();

signals:
	void serviceSelected(VeQItem *serviceRoot);

private:
	WINDOW *mTitleWindow = nullptr;
	WINDOW *mListViewWindow = nullptr;
	ListView *mListView = nullptr;
};

#endif
