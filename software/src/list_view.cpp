#include <QTimer>
#include "abstract_object_list_model.h"
#include "list_view.h"

ListView::ListView(WINDOW *w, QObject *parent):
	QObject(parent),
	mWindow(w),
	mRedrawTimer(new QTimer(this))
{
	mRedrawTimer->setInterval(100);
	mRedrawTimer->setSingleShot(true);
	connect(mRedrawTimer, SIGNAL(timeout()), this, SLOT(onRedrawAll()));
}

AbstractObjectListModel *ListView::model() const
{
	return mModel;
}

void ListView::setModel(AbstractObjectListModel *m)
{
	if (mModel == m)
		return;
	mModel = m;
	connect(mModel, SIGNAL(dataChanged(QModelIndex, QModelIndex)),
			this, SLOT(onDataChanged(QModelIndex, QModelIndex)));
	connect(mModel, SIGNAL(layoutChanged()),
			this, SLOT(onScheduleRedrawAll()));
	connect(mModel, SIGNAL(rowsInserted(QModelIndex, int, int)),
			this, SLOT(onScheduleRedrawAll()));
	connect(mModel, SIGNAL(rowsRemoved(QModelIndex, int, int)),
			this, SLOT(onScheduleRedrawAll()));
	connect(mModel, SIGNAL(modelReset()),
			this, SLOT(onScheduleRedrawAll()));
	onScheduleRedrawAll();
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

bool ListView::isEmphasized(int index) const
{
	Q_UNUSED(index);
	return false;
}

WINDOW *ListView::window() const
{
	return mWindow;
}

void ListView::redraw()
{
	if (mModel == nullptr)
		return;
	mSelectionIndex = qBound(0, mSelectionIndex, mModel->rowCount() - 1);
	int h = getListHeight();
	wmove(mWindow, 0, 0);
	int endIndex = qMin(mModel->rowCount(), h + mStartIndex);
	for (int r = mStartIndex; r < endIndex; ++r)
		_redrawRow(r);
	int y = endIndex - mStartIndex;
	if (y < h)
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

void ListView::onRedrawAll()
{
	redraw();
	wrefresh(mWindow);
}

void ListView::onScheduleRedrawAll()
{
	if (mRedrawTimer->isActive())
		return;
	mRedrawTimer->start();
}

int ListView::getListHeight() const
{
	return getmaxy(mWindow);
}

void ListView::_redrawRow(int index)
{
	if (index < mStartIndex || mModel == nullptr || index >= mModel->rowCount())
		return;
	int r = index - mStartIndex;
	if (r >= getListHeight())
		return;
	bool bold = isEmphasized(index);
	int attr = index == mSelectionIndex ? (bold ? COLOR_PAIR(3) : COLOR_PAIR(2)) : 0;
	if (bold)
		attr |= A_BOLD;
	wattron(mWindow, attr);
	wmove(mWindow, r, 0);
	wclrtoeol(mWindow);
	wmove(mWindow, r, 0);
	int w = getmaxx(mWindow);
	drawRow(index, w);
	wattroff(mWindow, attr);
}
