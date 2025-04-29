#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/ve_qitems_dbus.hpp>

#include "application.h"
#include "arguments.h"
#include "favorites_list_model.h"
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
		connect(item, SIGNAL(stateChanged(VeQItem::State)),
				this, SLOT(onStateChanged()));
	}
	connect(mRoot, SIGNAL(childAdded(VeQItem*)), this, SLOT(onDBusItemAdded(VeQItem*)));

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
