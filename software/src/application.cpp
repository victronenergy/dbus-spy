#include <climits>
#include <cstdio>

#include <QCommandLineParser>

#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/ve_qitems_dbus.hpp>

#include "application.h"
#include "favorites_list_model.h"
#include "headless_runner.h"
#include "object_listview.h"
#include "object_list_model.h"
#include "objects_screen.h"
#include "services_screen.h"
#include "search_manager.h"

// ncurses clobbers QT's timeout
#ifdef timeout
#undef timeout
#endif

Application::Application(int &argc, char **argv):
	QCoreApplication(argc, argv)
{
	QCoreApplication::setApplicationVersion(VERSION);
}

Application::~Application()
{
	if (mTimer != nullptr)
		endwin();

	if (mSearchWindow != nullptr)
		delwin(mSearchWindow);
}

namespace {

int usageError(const QString &message)
{
	fprintf(stderr, "dbus-spy: %s\nTry 'dbus-spy --help' for more information.\n",
			qPrintable(message));
	return 2;
}

bool parseSeconds(const QString &s, int &ms)
{
	bool ok = false;
	double seconds = s.toDouble(&ok);
	// Written so that NaN fails too.
	if (!(ok && seconds >= 0.001 && seconds <= INT_MAX / 1000.0))
		return false;
	ms = int(seconds * 1000);
	return true;
}

} // namespace

int Application::init()
{
	QCommandLineParser parser;
	parser.setApplicationDescription(
		"Realtime view of the com.victronenergy services on the D-Bus.\n\n"
		"Without --dump or --watch, an interactive (curses) UI is started.\n"
		"With them, items are printed as 'service path value' lines, e.g.:\n"
		"  dbus-spy --dump system /Dc/Battery\n"
		"  dbus-spy --watch 'battery*' '*/Soc'");
	QCommandLineOption helpOption = parser.addHelpOption();
	QCommandLineOption versionOption = parser.addVersionOption();
	QCommandLineOption dbusOption("dbus", "D-Bus address: system (default), session or a bus address.",
								  "address", "system");
	QCommandLineOption introspectOption({"i", "introspect"},
		"Use introspect to build a tree of the pesky services that do not support GetValue on the root.");
	QCommandLineOption historyOption({"r", "history"}, "Show items whose path starts with 'History'.");
	QCommandLineOption dumpOption("dump", "Print the matching items once and exit.");
	QCommandLineOption watchOption("watch",
		"Print the current value of the matching items, then a timestamped line for every "
		"change, until ctrl-c.");
	QCommandLineOption textOption("text", "With --dump/--watch: print the formatted text (e.g. '12.3V') "
								  "instead of the raw value.");
	QCommandLineOption timeoutOption("timeout", "With --dump/--watch: seconds to wait for the services "
									 "to load (default: 10).", "seconds", "10");
	QCommandLineOption durationOption("duration", "With --watch: stop after this many seconds.", "seconds");
	QCommandLineOption countOption("count", "With --watch: stop after this many changes.", "n");
	parser.addOptions({dbusOption, introspectOption, historyOption, dumpOption, watchOption,
					   textOption, timeoutOption, durationOption, countOption});
	parser.addPositionalArgument("service", "With --dump/--watch: service(s) to show, e.g. 'system', "
								 "'battery*' or a full name. Default: all; with --watch, the favorites "
								 "saved in the UI.", "[service]");
	parser.addPositionalArgument("path", "With --dump/--watch: path(s) to show, e.g. '/Dc/Battery' "
								 "(includes everything below it) or '*/Soc'. Default: all.", "[path]");
	// parse() instead of process(), so that a bad option exits with 2 like the other usage errors.
	if (!parser.parse(arguments()))
		return usageError(parser.errorText());
	if (parser.isSet(helpOption) || parser.isSet("help-all"))
		parser.showHelp();
	if (parser.isSet(versionOption))
		parser.showVersion();

	mUseIntrospect = parser.isSet(introspectOption);
	mShowHistory = parser.isSet(historyOption);
	bool headless = parser.isSet(dumpOption) || parser.isSet(watchOption);

	HeadlessOptions headlessOptions;
	if (headless) {
		const QStringList positional = parser.positionalArguments();
		if (positional.size() > 2)
			return usageError("too many arguments, expected at most [service] [path]");
		headlessOptions.watch = parser.isSet(watchOption);
		headlessOptions.text = parser.isSet(textOption);
		headlessOptions.showHistory = mShowHistory;
		headlessOptions.servicePattern = positional.value(0);
		headlessOptions.pathPattern = positional.value(1);
		if (headlessOptions.watch && positional.isEmpty()) {
			headlessOptions.favorites = FavoritesListModel::storedPaths();
			if (headlessOptions.favorites.isEmpty())
				return usageError("no favorites saved; add them with 'f' in the UI, or give a service ('*' for all)");
		}
		if (!parseSeconds(parser.value(timeoutOption), headlessOptions.timeoutMs))
			return usageError("--timeout expects a number of seconds");
		if (parser.isSet(durationOption) &&
			!parseSeconds(parser.value(durationOption), headlessOptions.durationMs))
			return usageError("--duration expects a number of seconds");
		if (parser.isSet(countOption)) {
			bool ok = false;
			headlessOptions.maxCount = parser.value(countOption).toInt(&ok);
			if (!ok || headlessOptions.maxCount <= 0)
				return usageError("--count expects a positive number");
		}
	}

	QString dbusAddress = parser.value(dbusOption);

	auto producer = new VeQItemDbusProducer{VeQItems::getRoot(), "dbus", true, true, this};
	if (!producer->open(dbusAddress)) {
		fprintf(stderr, "dbus-spy: could not connect to D-Bus '%s'\n", qPrintable(dbusAddress));
		return 2;
	}
	mRoot = producer->services();

	for (int i=0;;++i) {
		VeQItem *item = mRoot->itemChild(i);
		if (item == nullptr)
			break;
		connect(item, SIGNAL(stateChanged(VeQItem::State)),
				this, SLOT(onStateChanged()));
	}
	connect(mRoot, SIGNAL(childAdded(VeQItem*)), this, SLOT(onDBusItemAdded(VeQItem*)));

	if (headless) {
		// No curses and no favorites model: the latter creates the stored favorite items, which
		// would show up in the output even when they do not exist on the bus.
		auto runner = new HeadlessRunner{mRoot, producer, headlessOptions, this};
		connect(runner, &HeadlessRunner::finished, &QCoreApplication::exit);
		return 0;
	}

	mFavoritesModel = new FavoritesListModel{mRoot, this};
	mSearchManager = new SearchManager(this);

	initscr();
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_MAGENTA); // Title bar
	init_pair(2, COLOR_WHITE, COLOR_BLACK); // normal text
	init_pair(3, COLOR_CYAN, COLOR_BLACK); // selected text
	init_pair(4, COLOR_BLACK, COLOR_YELLOW); // search highlight
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

		// First, check if we're in search mode
		if (mSearchMode) {
			onSearchInput(c);
			continue;
		}

		// Pass input to the appropriate screen FIRST
		bool handled = false;
		if (mObjects != nullptr) {
			handled = mObjects->handleInput(c);
		} else if (mServices != nullptr) {
			handled = mServices->handleInput(c);
		} else if (mFavorites != nullptr) {
			handled = mFavorites->handleInput(c);
		}

		// Only if the screen didn't handle the input, process global keys
		if (!handled) {
			switch (c)
			{
			case '/':
				startSearch();
				break;
			case 'n':
				findNext();
				break;
			case 'N':
				findPrevious();
				break;
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

void Application::onStateChanged()
{
	VeQItem *item = static_cast<VeQItem *>(sender());
	Q_ASSERT(item->itemParent() == mRoot);
	if (item->getState() == VeQItem::Offline && item->itemChild(0) == nullptr) {
		mIncompatibleServices.insert(item->id());
		onDBusItemAdded(item);
		item->produceValue(QVariant(), VeQItem::Synchronized);
	}
}

void Application::startSearch()
{
	if (mSearchWindow == nullptr) {
		mSearchWindow = newwin(1, getmaxx(stdscr), getmaxy(stdscr) - 1, 0);
		keypad(mSearchWindow, TRUE);
	}

	wclear(mSearchWindow);
	wmove(mSearchWindow, 0, 0);
	waddstr(mSearchWindow, (char*)"/");
	wrefresh(mSearchWindow);

	mSearchMode = true;
	mSearchBuffer.clear();
	curs_set(1);
}

void Application::onSearchInput(wint_t c)
{
	switch (c) {
		case KEY_ENTER:
		case '\n':
			mSearchMode = false;
			curs_set(0);

			// Clear the search window
			wclear(mSearchWindow);
			wrefresh(mSearchWindow);

			if (!mSearchBuffer.isEmpty()) {
				mSearchManager->startSearch(mSearchBuffer);
				findNext();
			}
			break;

		case KEY_BACKSPACE:
		case 127: // Also backspace on some systems
			if (!mSearchBuffer.isEmpty()) {
				mSearchBuffer.chop(1);
				wclear(mSearchWindow);
				wmove(mSearchWindow, 0, 0);
				waddstr(mSearchWindow, (char*)"/");
				waddstr(mSearchWindow, mSearchBuffer.toUtf8().data());
				// Position cursor right after the text
				wmove(mSearchWindow, 0, 1 + mSearchBuffer.length());
				wrefresh(mSearchWindow);
			} else if (c == KEY_BACKSPACE || c == 127) {
				// Cancel search if backspace on empty search
				mSearchMode = false;
				curs_set(0);
			}
			break;

		case 27: // ESC key - cancel search
			mSearchMode = false;
			curs_set(0);
			break;

		default:
			// Add character to search buffer if it's printable
			if (c >= 32 && c <= 126) {
				mSearchBuffer += QChar(c);
				waddch(mSearchWindow, c);
				// Make sure cursor stays at the end of input
				wmove(mSearchWindow, 0, 1 + mSearchBuffer.length());
				wrefresh(mSearchWindow);
			}
			break;
	}

	if (!mSearchMode) {
		wclear(mSearchWindow);
		wrefresh(mSearchWindow);
		if (mObjects != nullptr)
			mObjects->repaint();
		else if (mServices != nullptr)
			mServices->repaint();
		else if (mFavorites != nullptr)
			mFavorites->repaint();
	}

	// Only refresh the main screen when exiting search mode
	// This prevents disrupting the cursor position during typing
	if (!mSearchMode) {
		refresh();
	}
}

void Application::findNext()
{
	if (!mSearchManager->hasPattern())
		return;

	AbstractObjectListModel *model = nullptr;
	ObjectListView *listView = nullptr;

	if (mObjects != nullptr) {
		model = mObjects->getModel();
		listView = mObjects->getListView();
	} else if (mServices != nullptr) {
		model = mServices->getModel();
		listView = mServices->getListView();
	} else if (mFavorites != nullptr) {
		model = mFavorites->getModel();
		listView = mFavorites->getListView();
	}

	if (model && listView) {
		int currentIndex = listView->getSelection();
		int newIndex = mSearchManager->findNext(model, currentIndex);

		// Get the search pattern
		QString pattern = mSearchManager->pattern();

		// Make sure we're in the right mode for status display
		curs_set(0);

		// Clear the status line at the bottom of the screen
		int y = getmaxy(stdscr) - 1;
		move(y, 0);
		clrtoeol();

		if (newIndex >= 0) {
			listView->setSelection(newIndex);
			attron(A_BOLD);
			mvprintw(y, 0, "Found: %s", pattern.toUtf8().data());
			attroff(A_BOLD);
		} else {
			attron(A_BOLD | COLOR_PAIR(4)); // Use search highlight color
			mvprintw(y, 0, "Pattern not found: %s", pattern.toUtf8().data());
			attroff(A_BOLD | COLOR_PAIR(4));
		}

		// Make sure to refresh both stdscr and the active window
		refresh();

		// Keep the message visible for a moment
		napms(300);

		// Redraw the active screen to ensure everything is updated
		if (mObjects != nullptr)
			mObjects->repaint();
		else if (mServices != nullptr)
			mServices->repaint();
		else if (mFavorites != nullptr)
			mFavorites->repaint();
	}
}

void Application::findPrevious()
{
	if (!mSearchManager->hasPattern())
		return;

	AbstractObjectListModel *model = nullptr;
	ObjectListView *listView = nullptr;

	if (mObjects != nullptr) {
		model = mObjects->getModel();
		listView = mObjects->getListView();
	} else if (mServices != nullptr) {
		model = mServices->getModel();
		listView = mServices->getListView();
	} else if (mFavorites != nullptr) {
		model = mFavorites->getModel();
		listView = mFavorites->getListView();
	}

	if (model && listView) {
		int currentIndex = listView->getSelection();
		int newIndex = mSearchManager->findPrevious(model, currentIndex);

		// Get the search pattern
		QString pattern = mSearchManager->pattern();

		// Make sure we're in the right mode for status display
		curs_set(0);

		// Clear the status line at the bottom of the screen
		int y = getmaxy(stdscr) - 1;
		move(y, 0);
		clrtoeol();

		if (newIndex >= 0) {
			listView->setSelection(newIndex);
			attron(A_BOLD);
			mvprintw(y, 0, "Found (reverse): %s", pattern.toUtf8().data());
			attroff(A_BOLD);
		} else {
			attron(A_BOLD | COLOR_PAIR(4)); // Use search highlight color
			mvprintw(y, 0, "Pattern not found (reverse): %s", pattern.toUtf8().data());
			attroff(A_BOLD | COLOR_PAIR(4));
		}

		// Make sure to refresh both stdscr and the active window
		refresh();

		// Keep the message visible for a moment
		napms(300);

		// Redraw the active screen to ensure everything is updated
		if (mObjects != nullptr)
			mObjects->repaint();
		else if (mServices != nullptr)
			mServices->repaint();
		else if (mFavorites != nullptr)
			mFavorites->repaint();
	}
}
