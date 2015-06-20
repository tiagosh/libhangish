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

#include <QNetworkCookie>
#include <QNetworkReply>
#include <QNetworkCookieJar>
#include <QDateTime>

#include "authenticator.h"
#include "channel.h"
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
    quint64 sendChatMessage(ClientSendChatMessageRequest clientSendChatMessageRequest);
    quint64 queryPresence(const QStringList &chatIds);
    void sendImage(QString segments, QString conversationId, QString filename);
    void sendCredentials(QString uname, QString passwd);
    void send2ndFactorPin(QString pin);
    void deleteCookies();
    void setActiveClient();
    void setFocus(QString convId, int status);
    void setTyping(QString convId, int status);
    quint64 setPresence(bool goingOnline);
    quint64 getConversation(ClientGetConversationRequest clientGetConversationRequest);
    void hangishDisconnect();
    void hangishConnect(quint64 lastKnownPushTs = 0);
    ClientEntity getMyself();
    QMap<QString, ClientEntity> getUsers();

public Q_SLOTS:
    void updateWatermark(QString convId);
    void onAuthenticationDone(QMap<QString, QNetworkCookie> cookies);
    void initDone();
    void onInitChatReply();
    void sendMessageReply();
    void queryPresenceReply();
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

Q_SIGNALS:
    void loginNeeded();
    void messageSent(quint64 requestId);
    void messageNotSent(quint64 requestId);
    void initFinished();
    void channelLost();
    void channelRestored();
    void authFailed(AuthenticationStatus status, QString error);
    void clientStateUpdate(ClientStateUpdate &csu);
    void clientSyncAllNewEventsResponse(ClientSyncAllNewEventsResponse &csanerp);
    void clientGetConversationResponse(quint64 requestId, ClientGetConversationResponse &cgcr);
    void clientSetPresenceResponse(quint64, ClientSetPresenceResponse &csprp);
    void clientQueryPresenceResponse(quint64, ClientQueryPresenceResponse &cqprp);

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
    quint64 mCurrentRequestId;
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
    QMap<QNetworkReply*, quint64> mPendingRequests;

};

#endif // HANGISHCLIENT_H
