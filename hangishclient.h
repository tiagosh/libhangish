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

#ifndef HANGISHCLIENT_H
#define HANGISHCLIENT_H

#include <QDateTime>

#include "authenticator.h"
#include "channel.h"
#include "utils.h"
#include "types.h"

class HangishClient : public QObject
{
    Q_OBJECT

public:
    HangishClient(const QString &cookiePath);
    QString getSelfChatId();
    ClientConversationState getConvById(const QString &cid);
    ClientEntity getUserById(QString chatId);
    void initChat(QString pvt);
    void sendChatMessage(QString segments, QString conversationId);
    void sendImage(QString segments, QString conversationId, QString filename);
    void sendCredentials(QString uname, QString passwd);
    void send2ndFactorPin(QString pin);
    void deleteCookies();
    void setActiveClient();
    void setFocus(QString convId, int status);
    void setTyping(QString convId, int status);
    void setPresence(bool goingOffline);
    void getConversation(ClientGetConversationRequest clientGetConversationRequest);
    void hangishDisconnect();
    void hangishConnect(quint64 lastKnownPushTs = 0);
    ClientEntity getMyself();

public Q_SLOTS:
    void updateWatermark(QString convId);
    void onAuthenticationDone(QMap<QString, QNetworkCookie> cookies);
    void initDone();
    void onInitChatReply();
    void sendMessageReply();
    void uploadImageReply();
    void uploadPerformedReply();
    void syncAllNewEventsReply();
    void setActiveClientReply();
    void setTypingReply();
    void setPresenceReply();
    void updateWatermarkReply();
    void getConversationReply();
    void updateClientId(QString newID);
    void setFocusReply();
    void cookieUpdateSlot(QNetworkCookie cookie);
    //void qnamUpdatedSlot(QNetworkAccessManager *qnam);

Q_SIGNALS:
    void loginNeeded();
    void messageSent();
    void messageNotSent();
    void initFinished();
    void channelLost();
    void channelRestored();
    void authFailed(AuthenticationStatus status, QString error);
    void clientStateUpdate(ClientStateUpdate &csu);
    void clientSyncAllNewEventsResponse(ClientSyncAllNewEventsResponse &csanerp);
    void clientGetConversationResponse(ClientGetConversationResponse &cgcr);

private Q_SLOTS:
    void onClientBatchUpdate(ClientBatchUpdate &cbu);
    void onGetPVTTokenReply();
    void onChannelRestored(quint64 lastRec);

private:
    void sendImageMessage(QString convId, QString imgId, QString segments);
    void performImageUpload(QString url);

    void getPVTToken();

    QString getRequestHeader();
    ClientRequestHeader *getRequestHeader1();
    void parseConversationState(QString conv);
    void followRedirection(QUrl url);

    QByteArray getAuthHeader();
    QNetworkReply * sendRequest(QString function, QString json);
    void syncAllNewEvents(quint64 timestamp);

    bool mAppPaused;
    bool mNeedSync;
    quint64 mLastKnownPushTs;
    QNetworkAccessManager mNetworkAccessManager;
    quint64 mNeedSyncTS;
    QDateTime mLastSetActive;
    QList<OutgoingImage> mOutgoingImages;
    QString mCookiePath;
    Authenticator *mAuthenticator;
    QNetworkCookieJar mCookieJar;
    QMap<QString, QNetworkCookie> mSessionCookies;
    QString mApiKey, mHeaderDate, mHeaderVersion, mHeaderId, mChannelPath, mClid, mChannelEcParam, mChannelPropParam, mSyncTimestamp;
    ClientEntity mMyself;
    Channel *mChannel;
    QMap<QString, ClientEntity> mUsers;
    QMap<QString, ClientConversationState> mConversations;
};

#endif // HANGISHCLIENT_H
