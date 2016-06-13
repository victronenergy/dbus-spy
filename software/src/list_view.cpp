#include <QAbstractItemModel>
#include "list_view.h"

ListView::ListView(WINDOW *w, QObject *parent):
	QObject(parent),
	mWindow(w),
	mModel(0),
	mStartIndex(0),
	mSelectionIndex(0)
{
}

QAbstractItemModel *ListView::model() const
{
	return mModel;
}

void ListView::setModel(QAbstractItemModel *m)
{
	if (mModel == m)
		return;
	mModel = m;
	connect(mModel, SIGNAL(dataChanged(QModelIndex, QModelIndex)),
			this, SLOT(onDataChanged(QModelIndex, QModelIndex)));
	connect(mModel, SIGNAL(layoutChanged()),
			this, SLOT(onLayoutChanged()));
	connect(mModel, SIGNAL(layoutAboutToBeChanged()),
			this, SLOT(onLayoutAboutToBeChanged()));
	connect(mModel, SIGNAL(rowsInserted(QModelIndex, int, int)),
			this, SLOT(onRowsInserted(QModelIndex, int, int)));
	connect(mModel, SIGNAL(rowsRemoved(QModelIndex, int, int)),
			this, SLOT(onRowsRemoved(QModelIndex, int, int)));
	connect(mModel, SIGNAL(modelReset()),
			this, SLOT(onModelReset()));
	connect(mModel, SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, row)),
			this, SLOT(onRowsMoved(QModelIndex, int, int, QModelIndex, row)));
	redraw();
	wrefresh(mWindow);
}

int ListView::getSelection() const
{
	return mSelectionIndex;
}

void ListView::setSelection(int s)
{
	if (mModel == 0)
		return;
	s = qBound(0, s, mModel->rowCount() - 1);
	if (s == mSelectionIndex)
		return;
	int h = getListHeight();
	int si = mStartIndex;
	while (si + h <= s)
		si += 5;
	while (si > s)
		si -= 5;
	if (si == mStartIndex) {
		int os = mSelectionIndex;
		mSelectionIndex = s;
		redrawRows(os, os);
		redrawRows(s, s);
	} else {
		mStartIndex = si;
		mSelectionIndex = s;
		redraw();
	}
	wrefresh(mWindow);
}

bool ListView::handleInput(int c)
{
	switch (c)
	{
	case KEY_DOWN:
		setSelection(mSelectionIndex + 1);
		break;
	case KEY_UP:
		setSelection(mSelectionIndex - 1);
		break;
	case KEY_NPAGE:
		setSelection(mSelectionIndex + getListHeight());
		break;
	case KEY_PPAGE:
		setSelection(mSelectionIndex - getListHeight());
		break;
	default:
		return false;
	}
	return true;
}

void ListView::drawRow(int index, int width) const
{
	QVariant v = mModel->data(mModel->index(index, 0));
	wprintw(mWindow, v.toString().left(width).toLatin1().data());
}

WINDOW *ListView::window() const
{
	return mWindow;
}

void ListView::redraw()
{
	if (mModel == 0)
		return;
	int h = getListHeight();
	wmove(mWindow, 0, 0);
	int endIndex = qMin(mModel->rowCount(), h + mStartIndex);
	for (int r = mStartIndex; r < endIndex; ++r)
		_redrawRow(r);
	wclrtobot(mWindow);
}

void ListView::redrawRows(int startIndex, int endIndex)
{
	if (endIndex < startIndex)
		return;
	int x = getcurx(mWindow);
	int y = getcury(mWindow);
	for (int i=startIndex; i <=endIndex; ++i)
		_redrawRow(i);
	wmove(mWindow, y, x);
}

void ListView::onDataChanged(const QModelIndex &topLeft,
							 const QModelIndex &bottomRight)
{
	redrawRows(topLeft.row(), bottomRight.row());
}

void ListView::onLayoutChanged()
{
	redraw();
	wrefresh(mWindow);
}

void ListView::onRowsInserted(const QModelIndex &parent, int first, int last)
{
	Q_UNUSED(parent);
	Q_UNUSED(last);
	redrawRows(first, mModel->rowCount() - 1);
	wrefresh(mWindow);
}

void ListView::onRowsRemoved(const QModelIndex &parent, int first, int last)
{
	Q_UNUSED(parent);
	Q_UNUSED(last);
	redrawRows(first, mModel->rowCount() - 1);
	int firstEmpty = mModel->rowCount() - mStartIndex;
	if (firstEmpty >= 0 && firstEmpty < getListHeight()) {
		wmove(mWindow, firstEmpty, 0);
		wclrtobot(mWindow);
	}
	wrefresh(mWindow);
}

void ListView::onModelReset()
{
	redraw();
	wrefresh(mWindow);
}

void ListView::onRowsMoved(const QModelIndex &parent, int start, int end,
						   const QModelIndex &destination, int row)
{
	Q_UNUSED(parent);
	Q_UNUSED(destination);
	redrawRows(qMin(start, row), qMax(end, row));
	wrefresh(mWindow);
}

int ListView::getListHeight() const
{
	return getmaxy(mWindow);
}

void ListView::_redrawRow(int index)
{
	if (index < mStartIndex || mModel == 0 || index >= mModel->rowCount())
		return;
	int r = index - mStartIndex;
	if (r >= getListHeight())
		return;
	if (index == mSelectionIndex)
		wattron(mWindow, COLOR_PAIR(2));
	wmove(mWindow, r, 0);
	wclrtoeol(mWindow);
	wmove(mWindow, r, 0);
	int w = getmaxx(mWindow);
	drawRow(index, w);
	if (index == mSelectionIndex)
		wattroff(mWindow, COLOR_PAIR(2));
}
