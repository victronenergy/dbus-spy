#ifndef VALUE_FORMAT_H
#define VALUE_FORMAT_H

#include <QString>
#include <QVariant>

/// Formats a D-Bus value for display: lists as [a,b], maps as {'k': v}, invalid values as '-'.
QString formatVariant(const QVariant &value);

#endif
