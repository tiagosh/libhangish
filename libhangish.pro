# The name of your application
QT += network
QT -= gui
TARGET = hangish
TEMPLATE = lib

SOURCES += authenticator.cpp \
    hangishclient.cpp \
    channel.cpp \
    utils.cpp 

HEADERS += \
    authenticator.h \
    hangishclient.h \
    channel.h \
    qtimports.h \
    utils.h \
    structs.h
