#include "search_manager.h"
#include "abstract_object_list_model.h"
#include <veutil/qt/ve_qitem.hpp>
#include "object_listview.h"

SearchManager::SearchManager(QObject *parent)
	: QObject(parent)
{
}

void SearchManager::startSearch(const QString &pattern)
{
	mSearchPattern = pattern;
	emit searchChanged(pattern);
}

int SearchManager::findNext(AbstractObjectListModel *model, int currentIndex)
{
	if (model == nullptr)
		return -1;

	if (mSearchPattern.isEmpty() || !model)
		return -1;

	int startIndex = currentIndex + 1;
	int count = model->rowCount();

	// Search from current position to end
	for (int i = startIndex; i < count; ++i) {
		VeQItem *item = model->getItem(i);
		if (isMatch(item, model)) {
			return i;
		}
	}

	// If not found, wrap around and search from beginning to current position
	for (int i = 0; i < startIndex; ++i) {
		VeQItem *item = model->getItem(i);
		if (isMatch(item, model)) {
			return i;
		}
	}

	return -1; // Not found
}

int SearchManager::findPrevious(AbstractObjectListModel *model, int currentIndex)
{
	if (model == nullptr)
		return -1;

	if (mSearchPattern.isEmpty() || !model)
		return -1;

	int count = model->rowCount();
	int startIndex = currentIndex - 1;
	if (startIndex < 0) {
		startIndex = count - 1;
	}

	// Search from current position to beginning
	for (int i = startIndex; i >= 0; --i) {
		VeQItem *item = model->getItem(i);
		if (isMatch(item, model)) {
			return i;
		}
	}

	// If not found, wrap around and search from end to current position
	for (int i = count - 1; i > startIndex; --i) {
		VeQItem *item = model->getItem(i);
		if (isMatch(item, model)) {
			return i;
		}
	}

	return -1; // Not found
}

bool SearchManager::isMatch(VeQItem *item, AbstractObjectListModel *model) const
{
	if (!item)
		return false;

	// Check if the path contains the search term
	QString path = model->getItemName(item);
	if (path.contains(mSearchPattern, Qt::CaseInsensitive)) {
		return true;
	}

	// Check if the value contains the search term
	if (item->isLeaf()) {
		QString value = item->getValue().toString();
		if (value.contains(mSearchPattern, Qt::CaseInsensitive)) {
			return true;
		}

		// Also check the text representation
		QString text = item->getText();
		if (text.contains(mSearchPattern, Qt::CaseInsensitive)) {
			return true;
		}
	}

	return false;
}
