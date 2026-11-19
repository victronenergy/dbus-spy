#include <cstdio>
#include <utility>

#include <QTime>
#include <QTimer>

#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/ve_qitems_dbus.hpp>

#include "headless_runner.h"
#include "value_format.h"

namespace {

// How often to check whether loading is done, and how long no new item may have appeared before
// we call it done. The quiet period covers services that are found or introspected late, which
// does not show up as a running GetItems call.
const int PollIntervalMs = 50;
const int QuietMs = 200;

// Shell-style glob: '*' matches anything (including '/'), '?' matches one character. Without a
// wildcard at the end, the pattern also matches everything below it (`subtreeSep`), so 'battery'
// matches com.victronenergy.battery.ttyO1 and '/Dc/Battery' matches /Dc/Battery/Soc.
QRegularExpression globToRegex(const QString &glob, QChar subtreeSep)
{
	QString re;
	for (QChar c: glob) {
		if (c == '*')
			re += ".*";
		else if (c == '?')
			re += '.';
		else
			re += QRegularExpression::escape(QString(c));
	}
	re = QString("^(?:%1)(?:%2.*)?$").arg(re, QRegularExpression::escape(QString(subtreeSep)));
	return QRegularExpression(re, QRegularExpression::CaseInsensitiveOption);
}

QString normalizeServicePattern(QString p)
{
	if (p.isEmpty())
		return "*";
	if (!p.startsWith("com.", Qt::CaseInsensitive))
		p.prepend("com.victronenergy.");
	return p;
}

QString normalizePathPattern(QString p)
{
	// No '/' in front of a leading '*': '*/Soc' must also match /Soc.
	if (!p.startsWith('/') && !p.startsWith('*'))
		p.prepend('/');
	while (p.size() > 1 && p.endsWith('/'))
		p.chop(1);
	return p == "/" ? QString("*") : p;
}

QString timestamp()
{
	return QTime::currentTime().toString("HH:mm:ss.zzz");
}

} // namespace

HeadlessRunner::HeadlessRunner(VeQItem *services, VeQItemDbusProducer *producer,
							   const HeadlessOptions &options, QObject *parent):
	QObject(parent),
	mServices(services),
	mProducer(producer),
	mOptions(options),
	mPollTimer(new QTimer(this))
{
	mOptions.pathPattern = normalizePathPattern(mOptions.pathPattern);
	// Explicitly asking for /History... implies --history.
	if (mOptions.pathPattern.startsWith("/History", Qt::CaseInsensitive))
		mOptions.showHistory = true;
	mServiceRx = globToRegex(normalizeServicePattern(mOptions.servicePattern), '.');
	mPathRx = globToRegex(mOptions.pathPattern, '/');

	mSinceStart.start();
	mSinceActivity.start();
	auto addService = [this](VeQItem *service) {
		if (mServiceRx.match(service->id()).hasMatch())
			addTree(service, service);
	};
	connect(mServices, &VeQItem::childAdded, this, addService);
	for (VeQItem *service: mServices->itemChildren())
		addService(service);

	connect(mPollTimer, &QTimer::timeout, this, &HeadlessRunner::onPoll);
	mPollTimer->start(PollIntervalMs);
}

void HeadlessRunner::addTree(VeQItem *item, VeQItem *service)
{
	mSinceActivity.restart();
	// Also listen on leaves: an item can have a value and children, e.g. /A and /A/B.
	if (item->isLeaf())
		addLeaf(item, service);
	connect(item, &VeQItem::childAdded, this, [this, service](VeQItem *child) { addTree(child, service); });
	for (VeQItem *child: item->itemChildren())
		addTree(child, service);
}

void HeadlessRunner::addLeaf(VeQItem *item, VeQItem *service)
{
	QString path = item->getRelId(service);
	if (!mOptions.showHistory && path.startsWith("/History"))
		return;
	if (!mPathRx.match(path).hasMatch())
		return;

	// No GetValue/GetText here: the service's bulk GetItems call delivers the values, also when
	// the service comes back after a restart.
	mLeaves.insert(item, {service, path, format(item)});
	if (mOptions.text)
		connect(item, &VeQItem::textChanged, this, [this, item] { onItemChanged(item); });
	else
		connect(item, &VeQItem::valueChanged, this, [this, item] { onItemChanged(item); });
	connect(item, &QObject::destroyed, this, [this, item] { mLeaves.remove(item); });
}

void HeadlessRunner::onItemChanged(VeQItem *item)
{
	Leaf &leaf = mLeaves[item];
	QString value = format(item);
	if (value == leaf.last)
		return;
	QString old = std::exchange(leaf.last, value);
	if (mLoading || !mOptions.watch || mStopped)
		return;

	if (!writeLine(QString("%1 %2 %3 %4 -> %5")
				   .arg(timestamp(), leaf.service->id(), leaf.path, old, value)))
		return;
	if (++mChangeCount == mOptions.maxCount)
		stop(0);
}

void HeadlessRunner::onPoll()
{
	QStringList pending = pendingServices();
	bool timedOut = mSinceStart.elapsed() >= mOptions.timeoutMs;
	if (!timedOut && (!pending.isEmpty() || mSinceActivity.elapsed() < QuietMs))
		return;
	if (!pending.isEmpty())
		fprintf(stderr, "dbus-spy: timed out waiting for: %s\n", qPrintable(pending.join(", ")));
	finishLoading();
}

void HeadlessRunner::finishLoading()
{
	mLoading = false;
	mPollTimer->stop();

	int count = printValues();
	if (mStopped)
		return;
	if (!mOptions.watch) {
		// Like grep: exit status 1 when nothing matched.
		stop(count == 0 ? 1 : 0);
		return;
	}
	if (count == 0)
		fprintf(stderr, "dbus-spy: no matching items (yet), waiting for them to appear\n");
	if (mOptions.durationMs > 0)
		QTimer::singleShot(mOptions.durationMs, this, [this] { stop(0); });
}

int HeadlessRunner::printValues()
{
	QStringList lines;
	for (auto it = mLeaves.begin(); it != mLeaves.end(); ++it) {
		// Read the value again: not every change is signalled, e.g. a null text becoming "".
		it->last = format(it.key());
		// Offline: the path does not (or no longer) exist on the service.
		if (it.key()->getState() != VeQItem::Offline)
			lines.append(QString("%1 %2 %3").arg(it->service->id(), it->path, it->last));
	}
	lines.sort();
	// A watch starts with the current values, timestamped like the changes that follow.
	QString prefix = mOptions.watch ? timestamp() + ' ' : QString();
	for (const QString &line: lines) {
		if (!writeLine(prefix + line))
			break;
	}
	return lines.size();
}

// Matching services whose bulk GetItems call is still running.
QStringList HeadlessRunner::pendingServices() const
{
	QStringList pending;
	for (VeDbusServicePrivate *service: mProducer->findChildren<VeDbusServicePrivate *>()) {
		// isGetItemsActive() can only be true once the service is attached to its root item,
		// which serviceName() needs.
		if (service->isGetItemsActive() && mServiceRx.match(service->serviceName()).hasMatch())
			pending.append(service->serviceName());
	}
	return pending;
}

QString HeadlessRunner::format(VeQItem *item) const
{
	// The local getters: the plain ones would send a GetValue/GetText for an Idle item.
	if (mOptions.text)
		return item->getTextState() == VeQItem::Synchronized ? item->getLocalText() : QString("-");
	return formatVariant(item->getLocalValue());
}

bool HeadlessRunner::writeLine(const QString &line)
{
	QByteArray bytes = line.toUtf8() + '\n';
	// Flush every line while watching, so the output works with tee/grep/a log file.
	if (fwrite(bytes.constData(), 1, bytes.size(), stdout) == size_t(bytes.size()) &&
		(!mOptions.watch || fflush(stdout) == 0))
		return true;
	stop(2); // e.g. a full disk
	return false;
}

void HeadlessRunner::stop(int exitCode)
{
	if (mStopped)
		return;
	mStopped = true;
	// A dump is only written here, when stdio flushes its buffer.
	if (fflush(stdout) != 0)
		exitCode = 2;
	emit finished(exitCode);
}
