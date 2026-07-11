#include <veutil/qt/ve_qitem.hpp>

#include "object_list_model.h"
#include "object_listview.h"
#include "services_screen.h"

ServicesScreen::ServicesScreen(VeQItem *root, QObject *parent):
	QObject(parent)
{
	mTitleWindow = newwin(1, getmaxx(stdscr), 0, 0);
	wmove(mTitleWindow, 0, 0);
	wattron(mTitleWindow, COLOR_PAIR(1));
	wprintw(mTitleWindow, "Services");
	wattroff(mTitleWindow, COLOR_PAIR(1));
	wrefresh(mTitleWindow);

	mListViewWindow = newwin(getmaxy(stdscr) - 2, getmaxx(stdscr), 1, 0);
	keypad(mListViewWindow, true);
	mModel = new ObjectListModel{root, false, true, this};
	mListView = new ObjectListView{mModel, mListViewWindow, this};
	refresh();
}

ServicesScreen::~ServicesScreen()
{
	delwin(mListViewWindow);
	delwin(mTitleWindow);
}

bool ServicesScreen::handleInput(wint_t c)
{
	switch (c)
	{
	case 'r':
		repaint();
		return true;
	case KEY_RIGHT:
	case KEY_ENTER:
	case '\n':
	{
		int s = mListView->getSelection();
		VeQItem *item = static_cast<ObjectListModel *>(mModel)->getItem(s);
		if (item != nullptr)
			emit serviceSelected(item);
		return true;
	}
	default:
		return mListView->handleInput(c);
	}
}

void ServicesScreen::repaint()
{
	wrefresh(mTitleWindow);
	wclear(mListViewWindow);
	mListView->redraw();
	wrefresh(mListViewWindow);
}

void ServicesScreen::resize()
{
	wresize(mTitleWindow, 1, getmaxx(stdscr));
	wresize(mListViewWindow, qMax(1, getmaxy(stdscr) - 2), qMax(1, getmaxx(stdscr)));
	// A row whose new content happens to match what curses already has cached for that
	// screen position (same text, just relaid out) would otherwise be skipped by
	// doupdate()'s diffing, and only reappear once something (like the selection) changes it.
	redrawwin(mTitleWindow);
	redrawwin(mListViewWindow);
	repaint();
}
