#ifndef LISTVIEW_H
#define LISTVIEW_H

#include <QObject>
#include <ncurses.h>

class AbstractObjectListModel;
class QModelIndex;
class QTimer;

class ListView : public QObject
{
	Q_OBJECT
public:
	ListView(WINDOW *w, QObject *parent = 0);

	AbstractObjectListModel *model() const;
	void setModel(AbstractObjectListModel *m);
	int getSelection() const;
	void setSelection(int s);
	virtual bool handleInput(int c);
	void redraw();
	void redrawRows(int startIndex, int endIndex);

protected:
	virtual void drawRow(int index, int width) const;
	virtual bool isEmphasized(int index) const;
	WINDOW *window() const;

private slots:
	void onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
	void onRedrawAll();
	void onScheduleRedrawAll();

private:
	int getListHeight() const;
	void _redrawRow(int index);

	WINDOW *mWindow	= nullptr;
	AbstractObjectListModel *mModel = nullptr;
	QTimer *mRedrawTimer = nullptr;
	int mStartIndex = 0;
	int mSelectionIndex = 0;
};

#endif
