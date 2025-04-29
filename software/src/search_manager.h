#ifndef SEARCH_MANAGER_H
#define SEARCH_MANAGER_H

#include <QObject>
#include <QString>
#include <QVector>

class AbstractObjectListModel;
class VeQItem;

class SearchManager : public QObject
{
	Q_OBJECT
public:
	explicit SearchManager(QObject *parent = nullptr);

	void startSearch(const QString &pattern);
	int findNext(AbstractObjectListModel *model, int currentIndex);
	int findPrevious(AbstractObjectListModel *model, int currentIndex);
	QString pattern() const { return mSearchPattern; }
	void clearSearch() { mSearchPattern.clear(); }
	bool hasPattern() const { return !mSearchPattern.isEmpty(); }

signals:
	void searchChanged(const QString &pattern);

private:
	bool isMatch(VeQItem *item, AbstractObjectListModel *model) const;

	QString mSearchPattern;
};

#endif // SEARCH_MANAGER_H
