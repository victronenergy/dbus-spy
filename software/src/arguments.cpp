#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>
#include "arguments.h"

Arguments::Arguments()
{
	QStringList argList = QCoreApplication::arguments();
	QString option;
	auto it = argList.begin();
	++it; // Skip application name
	for (; it != argList.end(); ++it)
	{
		QString arg = *it;
		if (arg.startsWith("--"))
		{
			option = arg.mid(2);
		}
		else if (arg.startsWith('-'))
		{
			option = arg.mid(1);
		}
		else
		{
			mArgList[option] = arg;
			option.clear();
		}
		if (!option.isEmpty())
			mArgList.insert(option, QString());
	}
}

void Arguments::print()
{
	QTextStream out{stdout};
	for (auto it = mArgList.begin(); it != mArgList.end(); ++it)
	{
		out << "key = " << it.key() << " value = " << it.value() << "\n";
	}
}

void Arguments::help()
{
	version();
	QTextStream out{stdout};
	out << endl
		<< "Options:" << endl;
	for (auto it = mHelp.begin(); it != mHelp.end(); ++it)
	{
		out << it.key() << endl
			<< "\t" << it.value() << endl;
	}
	out << endl;
}

void Arguments::version()
{
	QTextStream out{stdout};
	out << QCoreApplication::applicationName() << " v"
		<< QCoreApplication::applicationVersion() << endl;
}

void Arguments::addArg(const QString &arg, const QString &description)
{
	mHelp.insert(arg,description);
}
