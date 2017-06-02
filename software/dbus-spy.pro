# Application version
VERSION = 1.0.0

# suppress the mangling of va_arg has changed for gcc 4.4
QMAKE_CXXFLAGS += -Wno-psabi

# these warnings appear when compiling with QT4.8.3-debug. Problem appears to be
# solved in newer QT versions.
QMAKE_CXXFLAGS += -Wno-unused-local-typedefs

# Add more folders to ship with the application here
unix {
    bindir = $$(bindir)
    DESTDIR = $$(DESTDIR)
    isEmpty(bindir) {
        bindir = /usr/local/bin
    }
    INSTALLS += target
    target.path = $${DESTDIR}$${bindir}
}

MOC_DIR=.moc
OBJECTS_DIR=.obj

QT += core dbus xml
QT -= gui

TARGET = dbus-spy
CONFIG += console qdbus
CONFIG -= app_bundle
DEFINES += VERSION=\\\"$${VERSION}\\\"
QMAKE_CXXFLAGS += -std=c++11 # QT4 only, use CONFIG += c++11 for QT5

OBJECTS_DIR = .obj
MOC_DIR = .moc

TEMPLATE = app

LIBS += -lform -lncurses

include(ext/velib/src/qt/ve_qitems.pri)

INCLUDEPATH += \
    ext/velib/inc \
    src

SOURCES += \
    src/main.cpp \
    src/application.cpp \
    src/favorites_list_model.cpp \
    src/services_screen.cpp \
    src/objects_screen.cpp \
    src/list_view.cpp \
    src/object_listview.cpp \
    src/object_list_model.cpp \
    src/signal_handler.cpp \
    src/arguments.cpp

HEADERS += \
    src/abstract_object_list_model.h \
    src/application.h \
    src/favorites_list_model.h \
    src/services_screen.h \
    src/objects_screen.h \
    src/list_view.h \
    src/object_listview.h \
    src/object_list_model.h \
    src/signal_handler.h \
    src/arguments.h \
    velib/velib_config_app.h

DISTFILES += \
    ../README.md
