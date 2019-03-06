#include <QDebug>
#include <QtGlobal>
#include "application.h"

#include "signal_handler.h"

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
void myMessageOutput(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
	Q_UNUSED(type);
	Q_UNUSED(ctx);
	Q_UNUSED(msg);
}
#else
void myMessageOutput(QtMsgType type, const char *msg)
{
	Q_UNUSED(type);
	Q_UNUSED(msg);
}
#endif


int main(int argc, char *argv[])
{
	#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
	qInstallMessageHandler(myMessageOutput);
	#else
	qInstallMsgHandler(myMessageOutput);
	#endif

	Application a{argc, argv};

	SignalHandler h;
	h.add(SIGINT);
	h.add(SIGTERM);
	a.connect(&h, SIGNAL(signal(int)), &a, SLOT(quit()));

	int r = a.init();
	if (r != 0)
		return r;

	return a.exec();
}
