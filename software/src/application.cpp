#include <stdio.h>
#include <sys/poll.h>
#include <QDebug>
#include <QTimer>
#include <velib/qt/ve_qitem.hpp>
#include <velib/qt/ve_qitems_dbus.hpp>
#include <ncurses.h>
#include "application.h"
#include "arguments.h"
#include "objects_screen.h"
#include "services_screen.h"

Application::Application(int &argc, char **argv):
	QCoreApplication(argc, argv),
	mTimer(0),
	mRoot(0),
	mServices(0),
	mObjects(0)
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
	if (args.contains("h") || args.contains("help")) {
		args.help();
		return -1;
	}
	if (args.contains("v") || args.contains("version")) {
		args.version();
		return -1;
	}
	QString dbusAddress = args.contains("dbus") ? args.value("dbus") : "system";

	VeQItemDbusProducer *producer = new VeQItemDbusProducer(VeQItems::getRoot(), "dbus", true, true, this);
	producer->open(dbusAddress);
	mRoot = producer->services();
	// We need som extra code here to catch the com.victronenergy.settings service, because it does
	// not support a GetValue on the root, which is used by VeQItemDbusProducer to harvest all
	// existing items in the D-Bus services.
	for (int i=0;;++i) {
		VeQItem *item = mRoot->itemChild(i);
		if (item == 0)
			break;
		onDBusItemAdded(item);
	}
	connect(mRoot, SIGNAL(childAdded(VeQItem *)), this, SLOT(onDBusItemAdded(VeQItem *)));

	initscr();
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_MAGENTA);
	init_pair(2, COLOR_BLACK, COLOR_CYAN);
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
		bool handled = mObjects == 0 ? mServices->handleInput(c) : mObjects->handleInput(c);
		if (!handled) {
			switch (c)
			{
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
		mObjects->deleteLater();
		mObjects = 0;
	}
	mServices = new ServicesScreen(mRoot, this);
	connect(mServices, SIGNAL(serviceSelected(VeQItem *)),
			this, SLOT(onServiceSelected(VeQItem *)));
}

void Application::onServiceSelected(VeQItem *serviceRoot)
{
	if (mServices != 0) {
		mServices->deleteLater();
		mServices = 0;
	}
	Q_ASSERT(mObjects == 0);
	mObjects = new ObjectsScreen(serviceRoot, this);
	connect(mObjects, SIGNAL(goBack()), this, SLOT(onGoBack()));
}

void Application::onDBusItemAdded(VeQItem *item)
{
	if (item->uniqueId().contains("com.victronenergy.settings")) {
		connect(item, SIGNAL(childAdded(VeQItem *)), this, SLOT(onDBusItemAdded(VeQItem *)));
		static_cast<VeQItemDbus *>(item)->introspect();
	}
}
