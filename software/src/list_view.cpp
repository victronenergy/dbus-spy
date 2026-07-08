#include <QTimer>
#include "abstract_object_list_model.h"
#include "list_view.h"

// ncurses clobbers QT's timeout
#ifdef timeout
#undef timeout
#endif

ListView::ListView(WINDOW *w, QObject *parent):
	QObject(parent),
	mWindow(w),
	mRedrawTimer(new QTimer(this))
{
	mRedrawTimer->setInterval(100);
	mRedrawTimer->setSingleShot(true);
	connect(mRedrawTimer, SIGNAL(timeout()), SLOT(onRedrawAll()));
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
	connect(mModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
			this, SLOT(onDataChanged(QModelIndex,QModelIndex)));
	connect(mModel, SIGNAL(layoutChanged()),
			this, SLOT(onScheduleRedrawAll()));
	connect(mModel, SIGNAL(rowsInserted(QModelIndex,int,int)),
			this, SLOT(onScheduleRedrawAll()));
	connect(mModel, SIGNAL(rowsRemoved(QModelIndex,int,int)),
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
	// With a range active, moving the selection can change the highlighted state of rows other
	// than just the old/new selection (everything between the anchor and the new position), so
	// fall back to a full redraw instead of the two-row fast path.
	if (si == mStartIndex && mRangeAnchor == -1) {
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

void ListView::startRange()
{
	mRangeAnchor = mSelectionIndex;
	redraw();
	wrefresh(mWindow);
}

void ListView::cancelRange()
{
	if (mRangeAnchor == -1)
		return;
	mRangeAnchor = -1;
	redraw();
	wrefresh(mWindow);
}

bool ListView::hasRange() const
{
	return mRangeAnchor != -1;
}

int ListView::rangeAnchor() const
{
	return mRangeAnchor;
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
	case '\t':
	{
		int n = mModel == nullptr ? -1 : mModel->nextGroup(mSelectionIndex);
		if (n == -1)
			return false;
		setSelection(n);
		break;
	}
	case KEY_BTAB:
	{
		int p = mModel == nullptr ? -1 : mModel->previousGroup(mSelectionIndex);
		if (p == -1)
			return false;
		setSelection(p);
		break;
	}
	case ']':
	{
		int n = mModel == nullptr ? -1 : mModel->nextSection(mSelectionIndex);
		if (n == -1)
			return false;
		setSelection(n);
		break;
	}
	case '[':
	{
		int p = mModel == nullptr ? -1 : mModel->previousSection(mSelectionIndex);
		if (p == -1)
			return false;
		setSelection(p);
		break;
	}
	default:
		return false;
	}
	return true;
}

void ListView::drawRow(int index, int width) const
{
	QVariant v = mModel->data(mModel->index(index, 0));
	wprintw(mWindow, "%s", v.toString().left(width).toUtf8().data());
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
	if (mModel->rowCount() > 0)
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
	bool inRange = mRangeAnchor != -1
		&& index >= qMin(mRangeAnchor, mSelectionIndex)
		&& index <= qMax(mRangeAnchor, mSelectionIndex);
	int attr = index == mSelectionIndex ? (COLOR_PAIR(3) | A_STANDOUT) :
		inRange ? COLOR_PAIR(3) : COLOR_PAIR(2);
	if (isEmphasized(index))
		attr |= A_BOLD;
	wattron(mWindow, attr);
	wmove(mWindow, r, 0);
	wclrtoeol(mWindow);
	wmove(mWindow, r, 0);
	int w = getmaxx(mWindow);
	drawRow(index, w);
	wattroff(mWindow, attr);
}
