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
    hangishclient.h \
    types.h
