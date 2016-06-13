#ifndef OBJECTLISTVIEW_H
#define OBJECTLISTVIEW_H

#include <QList>
#include "list_view.h"

class ObjectListModel;
class VeQItem;

class ObjectListView : public ListView
{
	Q_OBJECT
public:
	ObjectListView(ObjectListModel *model, WINDOW *w, QObject *parent = 0);

	QString getValue(int index);

	void setValue(int index, const QVariant &v);

	bool showText() const;

	void setShowText(bool s);

protected:
	virtual void drawRow(int index, int width) const;

private slots:
	void onValueChanged();

	void onTextChanged();

	void onDestroyed();

private:
	void updateItem();

	VeQItem *getItem(int index) const;

	static QString convertVariant(const QVariant &value);
	mutable QList<VeQItem *> mItems;
	bool mShowText;
};

#endif // OBJECTLISTVIEW_H
