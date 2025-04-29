#ifndef APPLICATION_H
#define APPLICATION_H

#include <QCoreApplication>
#include <QSet>
#include <QString>
#include <cursesw.h> // Added for WINDOW type

class FavoritesListModel;
class ObjectListView;
class ObjectsScreen;
class QTimer;
class SearchManager;
class ServicesScreen;
class VeQItem;

class Application : public QCoreApplication
{
	Q_OBJECT

public:
	Application(int &argc, char **argv);
	~Application();

	int init();

	// Get the search manager instance
	SearchManager* searchManager() const { return mSearchManager; }

	// Start a search throughout the application
	void startSearch();

	// Search for next occurrence
	void findNext();

	// Search for previous occurrence
	void findPrevious();

private slots:
	void onCursesTimer();
	void onGoBack();
	void onGoToFavorites();
	void onLeaveFavorites();
	void onServiceSelected(VeQItem *serviceRoot);
	void onDBusItemAdded(VeQItem *item);
	void onStateChanged();
	void onSearchInput(wint_t c);

private:
	QTimer *mTimer = nullptr;
	VeQItem *mRoot = nullptr;
	FavoritesListModel *mFavoritesModel = nullptr;
	ServicesScreen *mServices = nullptr;
	ObjectsScreen *mObjects = nullptr;
	ObjectsScreen *mFavorites = nullptr;
	QSet<QString> mIncompatibleServices;
	QString mPrevPath;
	bool mUseIntrospect = false;
	bool mShowHistory = false;

	// Search related members
	SearchManager *mSearchManager = nullptr;
	bool mSearchMode = false;
	QString mSearchBuffer;
	WINDOW *mSearchWindow = nullptr;
};

#endif
