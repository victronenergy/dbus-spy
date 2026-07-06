#include <QByteArray>
#include <QDateTime>

#include <veutil/qt/ve_qitem.hpp>

#include "favorites_list_model.h"
#include "server.h"
#include "server_child.h"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

Server *Server::start(int port, QObject *parent)
{
	int fds[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
		std::printf("Server not started: socketpair failed (%s)\n", strerror(errno));
		return nullptr;
	}

	pid_t pid = fork();
	if (pid < 0) {
		std::printf("Server not started: fork failed (%s)\n", strerror(errno));
		close(fds[0]);
		close(fds[1]);
		return nullptr;
	}

	if (pid == 0) {
		// Detach from the terminal and everything else this process has open
		// (D-Bus connection included) before runServerChild() drops root --
		// it has no legitimate use for any of it.
		close(fds[0]);
		close(STDIN_FILENO);
		int devNull = open("/dev/null", O_RDONLY);
		if (devNull >= 0) {
			dup2(devNull, STDIN_FILENO);
			if (devNull != STDIN_FILENO)
				close(devNull);
		}

		// Opened here, while still root, so the fd stays writable after the
		// privilege drop below -- without this, lws's own error/warning
		// logging (which defaults to stderr) and our status line both
		// vanish into /dev/null, and a failed start is undiagnosable.
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		int logFd = open("/tmp/dbus-spy-server.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (logFd < 0)
			logFd = open("/dev/null", O_WRONLY);
		dup2(logFd, STDOUT_FILENO);
		dup2(logFd, STDERR_FILENO);
		if (logFd > STDERR_FILENO)
			close(logFd);

		setsid();
		runServerChild(fds[1], port);
	}

	close(fds[1]);
	signal(SIGCHLD, SIG_IGN); // let the kernel reap the child; we never wait() for it.

	QByteArray statusLine;
	struct pollfd pfd { fds[0], POLLIN, 0 };
	if (poll(&pfd, 1, 2000) > 0 && (pfd.revents & POLLIN)) {
		char buf[256];
		ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
		if (n > 0) {
			buf[n] = '\0';
			statusLine = QByteArray(buf).trimmed();
		}
	}

	if (statusLine.startsWith("OK")) {
		std::printf("Data feed listening on 127.0.0.1:%d (log: /tmp/dbus-spy-server.log)\n"
		            "Tunnel it: ssh -L %d:localhost:%d <this host>, then open software/graph.html\n"
		            "(edit its WS_HOST constant first if this port isn't the default, or if you're\n"
		            "serving the page from somewhere else, e.g. nginx on this device)\n",
		            port, port, port);
		return new Server(fds[0], parent);
	}

	std::printf("Server not started: %s (see /tmp/dbus-spy-server.log)\n",
	            statusLine.isEmpty() ? "no response from server child" : statusLine.constData());
	close(fds[0]);
	return nullptr;
}

Server::Server(int fd, QObject *parent):
	QObject(parent),
	mFd(fd)
{
	fcntl(mFd, F_SETFL, fcntl(mFd, F_GETFL, 0) | O_NONBLOCK);
	signal(SIGPIPE, SIG_IGN); // a dead/slow child must not take the whole app down
}

Server::~Server()
{
	if (mFd >= 0)
		close(mFd); // child sees EOF on its end and shuts itself down
}

void Server::attach(FavoritesListModel *favorites)
{
	mFavorites = favorites;
	connect(favorites, &QAbstractItemModel::rowsInserted,
	        this, &Server::onFavoritesRowsInserted);
	connect(favorites, &QAbstractItemModel::rowsAboutToBeRemoved,
	        this, &Server::onFavoritesRowsAboutToBeRemoved);
	for (int i = 0; i < favorites->rowCount(); ++i) {
		VeQItem *item = favorites->getItem(i);
		if (item != nullptr && !favorites->isServiceRoot(item))
			trackItem(item);
	}
}

void Server::onFavoritesRowsInserted(const QModelIndex &parent, int first, int last)
{
	Q_UNUSED(parent)
	for (int i = first; i <= last; ++i) {
		VeQItem *item = mFavorites->getItem(i);
		if (item != nullptr && !mFavorites->isServiceRoot(item))
			trackItem(item);
	}
}

void Server::onFavoritesRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
	Q_UNUSED(parent)
	for (int i = first; i <= last; ++i) {
		VeQItem *item = mFavorites->getItem(i);
		if (item == nullptr)
			continue;
		auto it = mTracked.find(item);
		if (it == mTracked.end())
			continue;
		// Without this, re-favoriting the same still-alive item later would
		// stack a second valueChanged/destroyed connection on top of this
		// one -- trackItem()'s mTracked.contains() guard only prevents that
		// for an item that was never removed, not one being re-added.
		disconnect(item, nullptr, this, nullptr);
		sendRemove(it.value());
		mTracked.erase(it);
	}
}

void Server::trackItem(VeQItem *item)
{
	if (mTracked.contains(item))
		return;
	mTracked.insert(item, item->uniqueId().toUtf8());
	connect(item, SIGNAL(valueChanged(QVariant)), this, SLOT(onValueChanged()));
	connect(item, SIGNAL(destroyed()), this, SLOT(onItemDestroyed()));
	sendSample(item);
}

void Server::onValueChanged()
{
	sendSample(static_cast<VeQItem *>(sender()));
}

void Server::onItemDestroyed()
{
	// sender() is mid-destruction here (VeQItem's own members are already
	// gone) -- only its identity as a QObject* is safe to use, which is why
	// mTracked stores the uniqueId separately instead of re-reading it now.
	auto it = mTracked.find(static_cast<VeQItem *>(sender()));
	if (it == mTracked.end())
		return;
	sendRemove(it.value());
	mTracked.erase(it);
}

void Server::sendSample(VeQItem *item)
{
	if (mFd < 0)
		return;
	bool ok = false;
	double value = item->getValue().toDouble(&ok);
	if (!ok)
		return; // only numeric favorites are graphable

	QByteArray line = "S\t" + item->uniqueId().toUtf8() + '\t' +
	                   QByteArray::number(QDateTime::currentMSecsSinceEpoch()) + '\t' +
	                   QByteArray::number(value) + '\n';
	// Best-effort: a slow or dead child just misses a sample, never blocks the TUI.
	write(mFd, line.constData(), line.size());
}

void Server::sendRemove(const QByteArray &path)
{
	if (mFd < 0)
		return;
	QByteArray line = "R\t" + path + '\n';
	write(mFd, line.constData(), line.size());
}
