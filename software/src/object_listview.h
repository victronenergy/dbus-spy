#ifndef OBJECTLISTVIEW_H
#define OBJECTLISTVIEW_H

#include <QList>
#include "list_view.h"

class AbstractObjectListModel;
class FavoritesListModel;
class VeQItem;

class ObjectListView : public ListView
{
	Q_OBJECT

public:
	ObjectListView(AbstractObjectListModel *model, WINDOW *w, QObject *parent = 0);

	QString getValue(int index);
	void setValue(int index, const QVariant &v);
	bool showText() const;
	void setShowText(bool s);
	void setFavorites(FavoritesListModel *favorites);
	void updateItem(VeQItem *item);

protected:
	void drawRow(int index, int width) const override;
	bool isEmphasized(int index) const override;

private slots:
	void onValueChanged();
	void onTextChanged();
	void onStateChanged();
	void onDestroyed();

private:
	VeQItem *getItem(int index) const;
	void registerItem(VeQItem *item) const;
	static QString convertVariant(const QVariant &value);

	mutable QList<VeQItem *> mItems;
	FavoritesListModel *mFavorites = nullptr;
	bool mShowText = false;
};

#endif
