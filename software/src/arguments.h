#ifndef ARGUMENTS_H
#define ARGUMENTS_H

#include <QMap>
#include <QString>

class Arguments
{
public:
	Arguments();

	bool contains(const QString &option) const
	{
		return mArgList.contains(option);
	}

	QString value(const QString &option) const
	{
		return mArgList.value(option);
	}

	void print();
	void help();
	void version();
	void addArg(const QString &arg, const QString &description);

private:
	QMap<QString, QString> mArgList;
	QMap<QString, QString> mHelp;
};

#endif
