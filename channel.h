/*

Hanghish
Copyright (C) 2015 Daniele Rogora

This file is part of Hangish.

Hangish is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Hangish is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Nome-Programma.  If not, see <http://www.gnu.org/licenses/>

*/


#ifndef CHANNEL_H
#define CHANNEL_H

#include <QObject>
#include <QNetworkReply>
#include <QNetworkCookieJar>
#include <QNetworkCookie>
#include <QTimer>

#include "utils.h"

class Channel : public QObject
{
    Q_OBJECT

public:
    Channel(QNetworkAccessManager *n, QList<QNetworkCookie> cookies, QString ppath, QString pclid, QString pec, QString pprop, User pms);
    void listen();
    void parseChannelData(QString sreply);
    QDateTime getLastPushTs();
    void fastReconnect();
    void setAppPaused();
    void setAppOpened();

public Q_SLOTS:
    void checkChannel();
    void networReadyRead();
    void networkRequestFinished();
    void parseSid();
    void slotError(QNetworkReply::NetworkError err);

private Q_SLOTS:
    void longPollRequest();

Q_SIGNALS:
    void cookieUpdateNeeded(QNetworkCookie cookie);
    void qnamUpdated(QNetworkAccessManager *qnam);
    void updateClientId(QString newID);
    void activeClientUpdate(int state);
    void updateWM(QString convId);
    void channelLost();
    void channelRestored(QDateTime lastRecv);
    void isTyping(QString convId, QString chatId, int type);
    void showNotification(QString preview, QString summary, QString body, QString sender);
    void incomingMessage(Event);

private:
    bool appPaused;
    ChannelEvent parseTypingNotification(QString input, ChannelEvent evt);
    void fetchNewSid();
    //Used to know wether the network has problems, so that the app doesn't spawn lots of unuseful connections
    QNetworkReply *mLongPoolRequest;
    bool mChannelError;
    QNetworkAccessManager *mNetworkAccessManager;
    User mMyself;
    QList<QNetworkCookie> mSessionCookies;
    QString mSid, mClid, mEc, mPath, mProp, mHeaderClient, mEmail, mGSessionId;
    QString mLastIncompleteParcel;
    QDateTime mLastPushReceived;
};

#endif // CHANNEL_H
