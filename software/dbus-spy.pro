# Application version
VERSION = 1.3.8

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

QT += core dbus xml
QT -= gui

CONFIG += console dbus
CONFIG -= app_bundle
DEFINES += VERSION=\\\"$${VERSION}\\\"

LIBS += -lformw -lncursesw -lwebsockets

equals(QT_MAJOR_VERSION, 6): QMAKE_CXXFLAGS += -std=c++17

# Local desktop dev builds pull libwebsockets/openssl from homebrew, which
# isn't on the default search path. The Venus SDK sysroot already has both,
# so this is a no-op there.
macx {
    INCLUDEPATH += $$system(brew --prefix libwebsockets)/include $$system(brew --prefix openssl@3)/include
    LIBS += -L$$system(brew --prefix libwebsockets)/lib -L$$system(brew --prefix openssl@3)/lib
}

SOURCES += \
    src/application.cpp \
    src/arguments.cpp \
    src/favorites_list_model.cpp \
    src/server_child.cpp \
    src/server.cpp \
    src/list_view.cpp \
    src/main.cpp \
    src/object_list_model.cpp \
    src/object_listview.cpp \
    src/objects_screen.cpp \
    src/services_screen.cpp \
    src/signal_handler.cpp \
    src/search_manager.cpp \

HEADERS += \
    src/abstract_object_list_model.h \
    src/application.h \
    src/arguments.h \
    src/favorites_list_model.h \
    src/server_child.h \
    src/server_common.h \
    src/server.h \
    src/list_view.h \
    src/object_list_model.h \
    src/object_listview.h \
    src/objects_screen.h \
    src/services_screen.h \
    src/signal_handler.h \
    src/search_manager.h \

DISTFILES += \
    ../README.md \
    graph.html


QMAKE_CXXFLAGS *= -ffunction-sections
QMAKE_LFLAGS *= -Wl,--gc-sections

!lessThan(QT_VERSION, 5) {
    QMAKE_CXXFLAGS += "-Wsuggest-override"
    CONFIG(debug, debug|release) {
        QMAKE_CXXFLAGS += "-Werror=suggest-override"
    }
}

include(ext/veutil/src/qt/veqitem.pri)
