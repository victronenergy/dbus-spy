#ifndef LISTVIEW_H
#define LISTVIEW_H

#include <QObject>
#include <ncurses.h>

class QAbstractItemModel;
class QModelIndex;
class QTimer;

class ListView : public QObject
{
	Q_OBJECT
public:
	ListView(WINDOW *w, QObject *parent = 0);

	QAbstractItemModel *model() const;

	void setModel(QAbstractItemModel *m);

	int getSelection() const;

	void setSelection(int s);

	virtual bool handleInput(int c);

	void redraw();

	void redrawRows(int startIndex, int endIndex);

protected:
	virtual void drawRow(int index, int width) const;

	WINDOW *window() const;

private slots:
	void onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

	void onRedrawAll();

	void onScheduleRedrawAll();

private:
	int getListHeight() const;

	void _redrawRow(int index);

	WINDOW *mWindow;
	QAbstractItemModel *mModel;
	QTimer *mRedrawTimer;
	int mStartIndex;
	int mSelectionIndex;
};

#endif // LISTVIEW_H
