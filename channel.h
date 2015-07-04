/**
 * libhangish
 * Copyright (C) 2015 Tiago Salem Herrmann
 * Copyright (C) 2015 Daniele Rogora
 *
 * This file is part of libhangish.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
    Channel(const QMap<QString, QNetworkCookie> &cookies, const QString &ppath, const QString &pclid, const QString &pec, const QString &pprop, ClientEntity pms);
    void listen();
    quint64 getLastPushTs() const;

private Q_SLOTS:
    void longPollRequest();
    void onChannelLost();
    void networReadyRead();
    void networkRequestFinished();
    void onFetchNewSidReply();
    void slotError(QNetworkReply::NetworkError err);

Q_SIGNALS:
    void cookieUpdateNeeded(QNetworkCookie cookie);
    void updateClientId(QString newID);
    void channelLost();
    void channelRestored(quint64 lastTimestamp);
    void clientBatchUpdate(ClientBatchUpdate &event);

private:
    void fetchNewSid();
    void parseChannelData(const QString &sreply);
    void processCookies(QNetworkReply *reply);

    QNetworkReply *mLongPoolRequest;
    bool mChannelError;
    QNetworkAccessManager mNetworkAccessManager;
    ClientEntity mMyself;
    QMap<QString, QNetworkCookie> mSessionCookies;
    QString mSid, mClid, mEc, mPath, mProp, mHeaderClient, mEmail, mGSessionId;
    quint64 mLastPushReceived;
    int mPendingParcelSize;
    QByteArray mPendingParcelBuffer;
    QTimer *mCheckChannelTimer;
    bool mFetchingSid;
};

#endif // CHANNEL_H
