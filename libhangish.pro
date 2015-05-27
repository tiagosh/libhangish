QT += network script xml
QT -= gui
TARGET = hangish
TEMPLATE = lib

CONFIG += link_pkgconfig
PKGCONFIG += protobuf

PROTOS = hangouts.proto
include(protobuf.pri)

INCLUDEPATH += $$OUT_PWD

SOURCES += authenticator.cpp \
    hangishclient.cpp \
    channel.cpp \
    utils.cpp

HEADERS += \
    hangishclient.h \
    channel.h \
    authenticator.h \
    types.h \
    utils.h

astyle.params += --style=linux
astyle.params += -z2
astyle.params += -c
astyle.params += --add-brackets
astyle.params += -H
astyle.commands = cd $$PWD; astyle $$astyle.params $$HEADERS $$SOURCES

QMAKE_EXTRA_TARGETS += astyle
exists("/usr/bin/astyle") {
    PRE_TARGETDEPS += astyle
}
