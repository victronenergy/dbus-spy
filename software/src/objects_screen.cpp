#include <veutil/qt/ve_qitem.hpp>

#include "favorites_list_model.h"
#include "objects_screen.h"
#include "object_list_model.h"
#include "object_listview.h"

ObjectsScreen::ObjectsScreen(const QString &title, AbstractObjectListModel *model,
							 FavoritesListModel *favorites, QObject *parent):
	QObject(parent),
	mModel(model),
	mFavorites(favorites)
{
	mTitleWindow = newwin(1, getmaxx(stdscr), 0, 0);
	wmove(mTitleWindow, 0, 0);
	wattron(mTitleWindow, COLOR_PAIR(1));
	wprintw(mTitleWindow, "%s", title.toUtf8().data());
	wattroff(mTitleWindow, COLOR_PAIR(1));
	wrefresh(mTitleWindow);

	mListViewWindow = newwin(getmaxy(stdscr) - 2, getmaxx(stdscr), 1, 0);
	keypad(mListViewWindow, true);
	mListView = new ObjectListView{model, mListViewWindow, this};
	mListView->setFavorites(mFavorites);
	if (model != favorites)
		mListView->setFavorites(favorites);
	refresh();
}

ObjectsScreen::~ObjectsScreen()
{
	delwin(mTitleWindow);
	delwin(mListViewWindow);
}

bool ObjectsScreen::handleInput(wint_t c)
{
	if (mEditForm == nullptr)
	{
		switch (c)
		{
		case KEY_LEFT:
			emit goBack();
			return true;
		case 'r':
			repaint();
			return true;
		case KEY_ENTER:
		case '\n':
			startEdit("Edit value: ", mListView->getValue(mListView->getSelection()));
			refresh();
			return true;
		case 'f':
		{
			int row = mListView->getSelection();
			VeQItem *item = mListView->model()->getItem(row);
			if (mFavorites->hasItem(item))
				mFavorites->removeItem(item);
			else
				mFavorites->addItem(item);
			/// @todo EV Ugly construction. updateItem should be called by the listview via signals
			/// from mFavorites.
			mListView->updateItem(item);
			return true;
		}
		case 'g':
		{
			int row = mListView->getSelection();
			VeQItem *item = mListView->model()->getItem(row);
			if (mListView->showText())
				item->getText(true);
			else
				item->getValue(true);
			return true;
		}
		case 't':
			mListView->setShowText(!mListView->showText());
			refresh();
			return true;
		default:
			return mListView->handleInput(c);
		}
	}
	else
	{
		switch (c)
		{
		case KEY_LEFT:
			c = REQ_PREV_CHAR;
			break;
		case KEY_RIGHT:
			c = REQ_NEXT_CHAR;
			break;
		case KEY_HOME:
			c = REQ_BEG_LINE;
			break;
		case KEY_END:
			c = REQ_END_LINE;
			refresh();
			break;
		case KEY_BACKSPACE:
		case 127:
			c = REQ_DEL_PREV;
			break;
		case 0x014A: // delete key?
			c = REQ_DEL_CHAR;
			break;
		case KEY_ENTER:
		case '\n':
		{
			QVariant qv;
			QString v = getEditValue();
			bool ok = false;
			if (v.startsWith("[") && v.endsWith("]")) {
				v = v.mid(1, v.size() - 2);
				QStringList parts = v.split(',', Qt::SkipEmptyParts);

				const bool allInts = std::all_of(parts.begin(), parts.end(),
        			[](const QString& v){ bool ok = false; v.toLong(&ok); return ok; });
				const bool allDoubles = std::all_of(parts.begin(), parts.end(),
					[](const QString& v){ bool ok = false; v.toDouble(&ok); return ok; });

				if (allInts) {
					QList<int> ints;
					for (const QString &part : parts)
						ints.append(part.toLong());
					qv = QVariant::fromValue(ints);
				} else if (allDoubles) {
					QList<double> doubles;
					for (const QString &part : parts)
						doubles.append(part.toDouble());
					qv = QVariant::fromValue(doubles);
				} else {
					QStringList list;
					for (const QString &part : parts)
						list.append(part);
					qv = QVariant::fromValue(list);
				}
			} else {
				int i = v.toLong(&ok);
				if (ok) {
					qv = i;
				} else {
					double d = v.toDouble(&ok);
					if (ok) {
						qv = d;
					} else {
						qv = v;
					}
				}
			}
			mListView->setValue(mListView->getSelection(), qv);
			endEdit();
			refresh();
			return true;
		}
		case 0x1B: // escape
			endEdit();
			refresh();
			return true;
		default:
			form_driver_w(mEditForm, OK, c);
			wrefresh(mEditWindow);
			return true;
		}

		if (c != 0) {
			form_driver(mEditForm, c);
			wrefresh(mEditWindow);
		}

		return true;
	}
}

void ObjectsScreen::repaint()
{
	wrefresh(mTitleWindow);
	wclear(mListViewWindow);
	mListView->redraw();
	wrefresh(mListViewWindow);
}

QString ObjectsScreen::getEditValue() const
{
	if (mEditForm == nullptr)
		return QString{};
	form_driver(mEditForm, REQ_VALIDATION);
	QString r = QString::fromUtf8(field_buffer(mEditFields[0], 0));
	return r.trimmed();
}

void ObjectsScreen::startEdit(const QString &description, const QString &text)
{
	if (mEditForm != nullptr)
		return;
	int y = getmaxy(stdscr) - 1;
	int x0 = description.size();
	mEditFields[0] = new_field(1, getmaxx(stdscr) - x0, 0, 0, 0, 0);
	mEditFields[1] = 0;
	set_field_buffer(mEditFields[0], 0, text.toUtf8().data());
	mEditForm = new_form(mEditFields);
	int rows = 0;
	int cols = 0;
	scale_form(mEditForm, &rows, &cols);
	mEditWindow = newwin(rows, cols, y, x0);
	keypad(mEditWindow, true);
	set_form_win(mEditForm, mEditWindow);
	set_form_sub(mEditForm, derwin(mEditWindow, rows, cols, 0, 0));
	post_form(mEditForm);
	wrefresh(mEditWindow);
	set_current_field(mEditForm, mEditFields[0]);
	curs_set(1);

	mvprintw(y, 0, "%s", description.toUtf8().data());
	wmove(mEditWindow, 0, 0);
}

void ObjectsScreen::endEdit()
{
	if (mEditForm == nullptr)
		return;
	curs_set(0);
	int y = getmaxy(stdscr) - 1;
	move(y, 0);
	clrtoeol();
	unpost_form(mEditForm);
	delwin(mEditWindow);
	free_form(mEditForm);
	free_field(mEditFields[0]);
	mEditForm = nullptr;
}
