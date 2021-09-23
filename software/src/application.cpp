#include <cursesw.h>

#include <velib/qt/ve_qitem.hpp>
#include <velib/qt/ve_qitems_dbus.hpp>

#include "application.h"
#include "arguments.h"
#include "favorites_list_model.h"
#include "object_list_model.h"
#include "objects_screen.h"
#include "services_screen.h"

Application::Application(int &argc, char **argv):
	QCoreApplication(argc, argv)
{
	QCoreApplication::setApplicationVersion(VERSION);
}

Application::~Application()
{
	if (mTimer != nullptr)
		endwin();
}

int Application::init()
{
	Arguments args;
	args.addArg("--dbus", "dbus address (session, system, ...)");
	args.addArg("-v --version", "Show version");
	args.addArg("-h --help", "Show help");
	args.addArg("-i --introspect", "Use introspect to build a tree of the pesky services support GetValue on the root");
	args.addArg("-r --history", "Show items whose path starts with 'History'.");
	if (args.contains("h") || args.contains("help")) {
		args.help();
		return -1;
	}
	if (args.contains("v") || args.contains("version")) {
		args.version();
		return -1;
	}
	mUseIntrospect = args.contains("i") || args.contains("introspect");
	mShowHistory = args.contains("r") || args.contains("history");

	QString dbusAddress = args.contains("dbus") ? args.value("dbus") : "system";

	auto producer = new VeQItemDbusProducer{VeQItems::getRoot(), "dbus", true, true, this};
	producer->open(dbusAddress);
	mRoot = producer->services();

	for (int i=0;;++i) {
		VeQItem *item = mRoot->itemChild(i);
		if (item == nullptr)
			break;
		connect(item, SIGNAL(stateChanged(VeQItem*,State)),
				this, SLOT(onStateChanged(VeQItem*)));
	}
	connect(mRoot, SIGNAL(childAdded(VeQItem*)), this, SLOT(onDBusItemAdded(VeQItem*)));

	mFavoritesModel = new FavoritesListModel{mRoot, this};

	initscr();
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_MAGENTA); // Title bar
	init_pair(2, COLOR_WHITE, COLOR_BLACK); // normal text
	init_pair(3, COLOR_CYAN, COLOR_BLACK); // selected text
	cbreak();
	noecho();
	nodelay(stdscr, TRUE);
	keypad(stdscr, TRUE);
	curs_set(0);
	refresh();

	onGoBack();

	mTimer = new QTimer{this};
	mTimer->setInterval(100);
	mTimer->start();
	connect(mTimer, SIGNAL(timeout()), SLOT(onCursesTimer()));

	return 0;
}

void Application::onCursesTimer()
{
	for (;;) {
		wint_t c;
		int status = get_wch(&c);
		if (status == ERR)
			break;

		bool handled = false;
		if (mObjects != nullptr)
			handled = mObjects->handleInput(c);
		else if (mServices != nullptr)
			handled = mServices->handleInput(c);
		else if (mFavorites != nullptr)
			handled = mFavorites->handleInput(c);
		if (!handled) {
			switch (c)
			{
			case 'F':
				if (mFavorites == nullptr)
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
	if (mObjects != nullptr) {
		delete mObjects;
		mObjects = nullptr;
	}
	if (mFavorites != nullptr) {
		delete mFavorites;
		mFavorites = nullptr;
	}
	mPrevPath.clear();
	mServices = new ServicesScreen{mRoot, this};
	connect(mServices, SIGNAL(serviceSelected(VeQItem*)),
			this, SLOT(onServiceSelected(VeQItem*)), Qt::QueuedConnection);
}

void Application::onGoToFavorites()
{
	if (mFavorites != nullptr)
		return;
	if (mObjects != nullptr) {
		delete mObjects;
		mObjects = nullptr;
	}
	if (mServices != nullptr) {
		delete mServices;
		mServices = nullptr;
	}
	mFavorites = new ObjectsScreen{"Favorites", mFavoritesModel, mFavoritesModel, this};
	connect(mFavorites, SIGNAL(goBack()), this, SLOT(onGoBack()), Qt::QueuedConnection);
}

void Application::onLeaveFavorites()
{
	VeQItem *serviceroot = mPrevPath.isEmpty() ? nullptr : VeQItems::getRoot()->itemGet(mPrevPath);
	if (serviceroot == nullptr)
		onGoBack();
	else
		onServiceSelected(serviceroot);
}

void Application::onServiceSelected(VeQItem *serviceRoot)
{
	if (mServices != nullptr) {
		delete mServices;
		mServices = nullptr;
	}
	if (mFavorites != nullptr) {
		delete mFavorites;
		mFavorites = nullptr;
	}
	Q_ASSERT(mObjects == nullptr);
	auto model = new ObjectListModel{serviceRoot, true, mShowHistory};
	mPrevPath = serviceRoot->uniqueId();
	mObjects = new ObjectsScreen{serviceRoot->id(), model, mFavoritesModel, this};
	model->setParent(mObjects);
	connect(mObjects, SIGNAL(goBack()), this, SLOT(onGoBack()), Qt::QueuedConnection);
}

void Application::onDBusItemAdded(VeQItem *item)
{
	VeQItem *child = item;
	for (;;) {
		VeQItem *parent = child->itemParent();
		if (parent == nullptr)
			return;
		if (parent == mRoot)
			break;
		child = parent;
	}
	if (!mUseIntrospect || !mIncompatibleServices.contains(child->id()))
		return;
	if (item->isLeaf())
		item->getValue();
	connect(item, SIGNAL(childAdded(VeQItem*)), this, SLOT(onDBusItemAdded(VeQItem*)));
	static_cast<VeQItemDbus *>(item)->introspect();
}

void Application::onStateChanged(VeQItem *item)
{
	Q_ASSERT(item->itemParent() == mRoot);
	if (item->getState() == VeQItem::Offline && item->itemChild(0) == nullptr) {
		mIncompatibleServices.insert(item->id());
		onDBusItemAdded(item);
		item->produceValue(QVariant(), VeQItem::Synchronized);
	}
}
