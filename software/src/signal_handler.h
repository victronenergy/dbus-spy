#ifndef SIGNALHANDLER_H
#define SIGNALHANDLER_H

#include <QObject>
#include <signal.h>

class QSocketNotifier;

/// This class catches unix signal handlers and emit a signal in the main thread when they are
/// triggers.
/// We cannot call QT functions in signal handlers due to their asynchronous behavior, so we pass
/// the signal to a pair of unix sockets. We write to one in the signal handler, and read from the
/// other via a QSocketNotifier whose `activated` slot is conveniently fired from the QT thread.
/// Inspiration for this class has been taken from http://doc.qt.io/qt-4.8/unix-signals.html.
class SignalHandler : public QObject
{
	Q_OBJECT
public:
	SignalHandler(QObject *parent = nullptr);

	void add(int signal);

signals:
	void signal(int signal);

private slots:
	void handleSigTerm();

	static void termSignalHandler(int unused);

private:
	static int sigtermFd[2];

	QSocketNotifier *snTerm = nullptr;
};

#endif // SIGNALHANDLER_H
