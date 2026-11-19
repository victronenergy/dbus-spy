#ifndef HEADLESS_RUNNER_H
#define HEADLESS_RUNNER_H

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

class QTimer;
class VeQItem;
class VeQItemDbusProducer;

struct HeadlessOptions
{
	bool watch = false;
	bool text = false;
	bool showHistory = false;
	QString servicePattern;
	QString pathPattern;
	QStringList favorites; // '/service/path' items to watch instead of the patterns
	int timeoutMs = 10000;
	int durationMs = -1; // watch: stop after this long (-1: run until interrupted)
	int maxCount = -1;   // watch: stop after this many changes (-1: unlimited)
};

/// Non-interactive mode (--dump / --watch): prints D-Bus items as plain text lines instead of
/// showing the curses UI.
class HeadlessRunner : public QObject
{
	Q_OBJECT
public:
	HeadlessRunner(VeQItem *services, VeQItemDbusProducer *producer,
				   const HeadlessOptions &options, QObject *parent = nullptr);

signals:
	void finished(int exitCode);

private:
	struct Leaf
	{
		VeQItem *service = nullptr;
		QString path;
		int favorite = -1; // position in the favorites; sorts the initial values (-1: not a favorite)
		QString last; // last formatted value, used to suppress repeats and to show old -> new
	};

	void addTree(VeQItem *item, VeQItem *service);
	void addLeaf(VeQItem *item, VeQItem *service);
	void onItemChanged(VeQItem *item);
	void onPoll();
	void finishLoading();
	int printValues();
	QStringList pendingServices() const;
	QString format(VeQItem *item) const;
	bool writeLine(const QString &line);
	void stop(int exitCode);

	VeQItem *mServices = nullptr;
	VeQItemDbusProducer *mProducer = nullptr;
	HeadlessOptions mOptions;
	QRegularExpression mServiceRx;
	QRegularExpression mPathRx;
	QHash<VeQItem *, Leaf> mLeaves;
	QTimer *mPollTimer = nullptr;
	QElapsedTimer mSinceStart;
	QElapsedTimer mSinceActivity;
	bool mLoading = true;
	bool mStopped = false;
	int mChangeCount = 0;
};

#endif
