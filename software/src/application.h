#ifndef APPLICATION_H
#define APPLICATION_H

#include <QCoreApplication>

class ListView;
class ObjectsScreen;
class QTimer;
class ServicesScreen;
class VeQItem;

class Application : public QCoreApplication
{
	Q_OBJECT
public:
	Application(int &argc, char **argv);

	~Application();

	int init();

private slots:
	void onCursesTimer();

	void onGoBack();

	void onServiceSelected(VeQItem *serviceRoot);

	void onDBusItemAdded(VeQItem *item);

private:
	QTimer *mTimer;
	VeQItem *mRoot;
	ServicesScreen *mServices;
	ObjectsScreen *mObjects;
};

#endif // APPLICATION_H
