#include <stdio.h>
#include <sys/poll.h>
#include <QDebug>
#include <QTimer>
#include <velib/qt/ve_qitem.hpp>
#include <velib/qt/ve_qitems_dbus.hpp>
#include <ncurses.h>
#include "application.h"
#include "arguments.h"
#include "favorites_list_model.h"
#include "object_list_model.h"
#include "objects_screen.h"
#include "services_screen.h"

/// This class will adapt a VeQItem and adjust its setValue class to make sure we always get out
/// of the Storing state.
template <typename BaseClass>
class NoStorageQItem: public BaseClass
{
public:
	using BaseClass::BaseClass;

	virtual int setValue(QVariant const &value)
	{
		int i = setValue(value);
		if (i != 0)
			return i;
		BaseClass::setState(VeQItem::Synchronized);
		return 0;
	}
};

/// This class allows you to take any class inheriting VeQItemProducer, and force it to create a
/// custom VeQItem. This item should inherit the item created by the VeQItemProducer specified
/// in BaseClass. If you do not do that, the producer may get confused.
template <typename BaseClass, typename Item>
class CustomQItemProducer: public BaseClass
{
public:
	using BaseClass::BaseClass;

	virtual VeQItem *createItem()
	{
		return new Item(this);
	}
};

//class NoStorageQItem: public VeQItemDbus
//{
//public:
//	NoStorageQItem(VeQItemDbusProducer *producer):
//		VeQItemDbus(producer)
//	{}

//	virtual int setValue(QVariant const &value)
//	{
//		int i = setValue(value);
//		if (i != 0)
//			return i;
//		setState(VeQItem::Synchronized);
//		return 0;
//	}
//};

//class NoStorageQItemProducer: public VeQItemDbusProducer
//{
//public:
//	NoStorageQItemProducer(VeQItem *root, QString id, bool findVictronServices = true,
//		bool bulkInitOfNewService = true, QObject *parent = 0):
//		VeQItemDbusProducer(root, id, findVictronServices, bulkInitOfNewService, parent)
//	{}


//	virtual VeQItem *createItem()
//	{
//		return new NoStorageQItem(this);
//	}
//};

Application::Application(int &argc, char **argv):
	QCoreApplication(argc, argv),
	mTimer(0),
	mRoot(0),
	mFavoritesModel(0),
	mServices(0),
	mObjects(0),
	mFavorites(0),
	mUseIntrospect(false),
        mInitCount(0)
{
	QCoreApplication::setApplicationVersion(VERSION);
}

Application::~Application()
{
	if (mTimer != 0)
		endwin();
}

int Application::init()
{
	Arguments args;
	args.addArg("--dbus", "dbus address (session, system, ...)");
	args.addArg("-v --version", "Show version");
	args.addArg("-h --help", "Show help");
	args.addArg("-i --introspect", "Use introspect to build a tree of the pesky services support GetValue on the root");
	if (args.contains("h") || args.contains("help")) {
		args.help();
		return -1;
	}
	if (args.contains("v") || args.contains("version")) {
		args.version();
		return -1;
	}
	mUseIntrospect = args.contains("i") || args.contains("introspect");

	QString dbusAddress = args.contains("dbus") ? args.value("dbus") : "system";

	VeQItemDbusProducer *producer = new CustomQItemProducer<VeQItemDbusProducer, VeQItemDbus>(VeQItems::getRoot(), "dbus", true, true, this);
	producer->open(dbusAddress);
	mRoot = producer->services();
	// We need som extra code here to catch the com.victronenergy.settings service, because it does
	// not support a GetValue on the root, which is used by VeQItemDbusProducer to harvest all
	// existing items in the D-Bus services.
	for (int i=0;;++i) {
		VeQItem *item = mRoot->itemChild(i);
		if (item == 0)
			break;
		connect(item, SIGNAL(stateChanged(VeQItem *, State)),
				this, SLOT(onStateChanged(VeQItem *)));
		++mInitCount;
	}
	connect(mRoot, SIGNAL(childAdded(VeQItem *)), this, SLOT(onDBusItemAdded(VeQItem *)));

	initscr();
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_MAGENTA); // Title bar
	init_pair(2, COLOR_BLACK, COLOR_CYAN); // Selected
	init_pair(3, COLOR_WHITE, COLOR_CYAN); // Selected and favorite
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	curs_set(0);
	refresh();

	onGoBack();

	mTimer = new QTimer(this);
	mTimer->setInterval(100);
	mTimer->start();
	connect(mTimer, SIGNAL(timeout()), this, SLOT(onCursesTimer()));

	return 0;
}

void Application::onCursesTimer()
{
	pollfd fds;
	fds.fd = 0; // STDIN
	fds.events = POLLIN;
	for (;;) {
		int ret = poll(&fds, 1, 0);
		if (ret != 1)
			return;
		int c = getch();
		bool handled = false;
		if (mObjects != 0)
			handled = mObjects->handleInput(c);
		else if (mServices != 0)
			handled = mServices->handleInput(c);
		else if (mFavorites != 0)
			handled = mFavorites->handleInput(c);
		if (!handled) {
			switch (c)
			{
			case 'F':
				if (mFavorites == 0)
					onGoToFavorites();
				else
					onLeaveFavorites();
				break;
			case 'q':
				quit();
				break;
			default:
				break;
			}
		}
	}
}

void Application::onGoBack()
{
	if (mObjects != 0) {
		delete mObjects;
		mObjects = 0;
	}
	if (mFavorites != 0) {
		delete mFavorites;
		mFavorites = 0;
	}
	mPrevPath.clear();
	mServices = new ServicesScreen(mRoot, this);
	connect(mServices, SIGNAL(serviceSelected(VeQItem *)),
			this, SLOT(onServiceSelected(VeQItem *)), Qt::QueuedConnection);
}

void Application::onGoToFavorites()
{
	if (mFavorites != 0)
		return;
	if (mObjects != 0) {
		delete mObjects;
		mObjects = 0;
	}
	if (mServices != 0) {
		delete mServices;
		mServices = 0;
	}
	mFavorites = new ObjectsScreen("Favorites", mFavoritesModel, mFavoritesModel, this);
	connect(mFavorites, SIGNAL(goBack()), this, SLOT(onGoBack()), Qt::QueuedConnection);
}

void Application::onLeaveFavorites()
{
	VeQItem *serviceroot = mPrevPath.isEmpty() ? 0 : VeQItems::getRoot()->itemGet(mPrevPath);
	if (serviceroot == 0)
		onGoBack();
	else
		onServiceSelected(serviceroot);
}

void Application::onServiceSelected(VeQItem *serviceRoot)
{
	if (mServices != 0) {
		delete mServices;
		mServices = 0;
	}
	if (mFavorites != 0) {
		delete mFavorites;
		mFavorites = 0;
	}
	Q_ASSERT(mObjects == 0);
	ObjectListModel *model = new ObjectListModel(serviceRoot, true);
	mPrevPath = serviceRoot->uniqueId();
	mObjects = new ObjectsScreen(serviceRoot->id(), model, mFavoritesModel, this);
	model->setParent(mObjects);
	connect(mObjects, SIGNAL(goBack()), this, SLOT(onGoBack()), Qt::QueuedConnection);
}

void Application::onDBusItemAdded(VeQItem *item)
{
	VeQItem *child = item;
	for (;;) {
		VeQItem *parent = child->itemParent();
		if (parent == 0)
			return;
		if (parent == mRoot)
			break;
		child = parent;
	}
	if (!mUseIntrospect || !mIncompatibleServices.contains(child->id()))
		return;
	if (item->isLeaf())
		item->getValue();
	connect(item, SIGNAL(childAdded(VeQItem *)), this, SLOT(onDBusItemAdded(VeQItem *)));
	static_cast<VeQItemDbus *>(item)->introspect();
}

void Application::onStateChanged(VeQItem *item)
{
	Q_ASSERT(item->itemParent() == mRoot);
	if (item->getState() == VeQItem::Offline && item->itemChild(0) == 0) {
		mIncompatibleServices.insert(item->id());
		onDBusItemAdded(item);
		item->produceValue(QVariant(), VeQItem::Synchronized);
	}
	if (mInitCount > 0 && mFavoritesModel == nullptr) {
		--mInitCount;
		if (mInitCount == 0)
			mFavoritesModel = new FavoritesListModel(mRoot, this);
	}
}
