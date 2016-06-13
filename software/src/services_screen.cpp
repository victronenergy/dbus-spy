#include <velib/qt/ve_qitem.hpp>
#include "object_list_model.h"
#include "object_listview.h"
#include "services_screen.h"

ServicesScreen::ServicesScreen(VeQItem *root, QObject *parent):
	QObject(parent),
	mTitleWindow(0),
	mListViewWindow(0),
	mListView(0)
{
	mTitleWindow = newwin(1, getmaxx(stdscr), 0, 0);
	wmove(mTitleWindow, 0, 0);
	wattron(mTitleWindow, COLOR_PAIR(1));
	wprintw(mTitleWindow, "Services");
	wattroff(mTitleWindow, COLOR_PAIR(1));
	wrefresh(mTitleWindow);

	mListViewWindow = newwin(getmaxy(stdscr) - 2, getmaxx(stdscr), 1, 0);
	keypad(mListViewWindow, true);
	ObjectListModel *model = new ObjectListModel(root, false, this);
	mListView = new ObjectListView(model, mListViewWindow, this);
	refresh();
}

ServicesScreen::~ServicesScreen()
{
	delwin(mListViewWindow);
	delwin(mTitleWindow);
}

bool ServicesScreen::handleInput(int c)
{
	switch (c)
	{
	case KEY_RIGHT:
	case KEY_ENTER:
	case '\n':
	{
		int s = mListView->getSelection();
		VeQItem *item = static_cast<ObjectListModel *>(mListView->model())->getItem(s);
		if (item != 0)
			emit serviceSelected(item);
		return true;
	}
	default:
		return mListView->handleInput(c);
	}
}
