#include <QDBusVariant>
#include <QStringList>

#include "value_format.h"

QString formatVariant(const QVariant &value)
{
	// Workaround: the standard demarshall function does not convert QBusVariant objects to
	// QVariants. This causes problems in com.victronenergy.system/pvinverterids.
	if (value.canConvert<QDBusVariant>())
	{
		QDBusVariant v = qvariant_cast<QDBusVariant>(value);
		return formatVariant(v.variant());
	}
	switch (value.typeId()) {
	case QMetaType::UChar:
		return QString::number(value.toUInt());
	case QMetaType::Double:
		return QString::number(value.toDouble());
	case QMetaType::UnknownType:
		return "-";
	case QMetaType::QStringList:
		return "[" + value.toStringList().join(",") + "]";
	case QMetaType::QVariantList:
	{
		QList<QVariant> list = value.toList();
		QString r = "[";
		for (QVariant &v: list)
		{
			r.append(formatVariant(v));
			r.append(',');
		}
		if (r.size() > 1)
			r.chop(1);
		r.append("]");
		return r;
	}
	case QMetaType::QVariantMap:
	{
		QMap<QString, QVariant> map = value.toMap();
		QString r = "{";
		for (auto it = map.begin(); it != map.end(); ++it)
			r.append(QString("'%1': %2,").arg(it.key(), formatVariant(it.value())));
		if (r.size() > 1)
			r.chop(1);
		r.append("}");
		return r;
	}
	default:
		return value.toString();
	}
}
