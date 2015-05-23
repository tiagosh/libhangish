# The name of your application
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
