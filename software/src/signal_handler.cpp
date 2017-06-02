#include <QCoreApplication>
#include <QDebug>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <QSocketNotifier>
#include "signal_handler.h"

int SignalHandler::sigtermFd[2];

SignalHandler::SignalHandler(QObject *parent):
	QObject(parent)
{
	if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigtermFd))
		qFatal("Couldn't create TERM socketpair");
	snTerm = new QSocketNotifier{sigtermFd[1], QSocketNotifier::Read, this};
	connect(snTerm, SIGNAL(activated(int)), this, SLOT(handleSigTerm()));
}

void SignalHandler::add(int signal)
{
	struct sigaction term;
	memset(&term, 0, sizeof(term));
	term.sa_handler = SignalHandler::termSignalHandler;
	sigemptyset(&term.sa_mask);
	term.sa_flags = SA_RESTART;

	if (sigaction(signal, &term, 0) > 0)
		qFatal("sigaction failed");
}

void SignalHandler::termSignalHandler(int signal)
{
	::write(sigtermFd[0], &signal, sizeof(signal));
}

void SignalHandler::handleSigTerm()
{
	snTerm->setEnabled(false);
	int sgn;
	::read(sigtermFd[1], &sgn, sizeof(sgn));

	emit signal(sgn);

	snTerm->setEnabled(true);
}
