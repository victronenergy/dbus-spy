#include <QDebug>
#include "application.h"

#include "signal_handler.h"

void myMessageOutput(QtMsgType type, const char *msg)
{
	Q_UNUSED(type);
	Q_UNUSED(msg);
}

//void myMessageOutput(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
//{
//	Q_UNUSED(type);
//	Q_UNUSED(ctx);
//	Q_UNUSED(msg);
//}

int main(int argc, char *argv[])
{
	// qInstallMessageHandler(myMessageOutput);
	qInstallMsgHandler(myMessageOutput);

	Application a(argc, argv);

	SignalHandler h;
	h.add(SIGINT);
	h.add(SIGTERM);
	a.connect(&h, SIGNAL(signal(int)), &a, SLOT(quit()));

	int r = a.init();
	if (r != 0)
		return r;

	return a.exec();
}
