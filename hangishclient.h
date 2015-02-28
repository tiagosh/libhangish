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


#ifndef CLIENT_H
#define CLIENT_H

#include <QDateTime>
#include <QCryptographicHash>

#include "authenticator.h"
#include "channel.h"
#include "utils.h"

class HangishClient : public QObject
{
    Q_OBJECT

public:
    HangishClient(const QString &cookiePath);
    void initChat(QString pvt);
    Conversation getConvById(const QString &cid);
    void sendChatMessage(QString segments, QString conversationId);
    void sendImage(QString segments, QString conversationId, QString filename);
    QString getSelfChatId();
    void sendCredentials(QString uname, QString passwd);
    void send2ndFactorPin(QString pin);
    void deleteCookies();
    void testNotification();
    void forceChannelRestore();
    void setActiveClient();
    void setFocus(QString convId, int status);
    void setTyping(QString convId, int status);
    void setPresence(bool goingOffline);
    void setAppPaused();
    void setAppOpened();


public Q_SLOTS:
    void updateWatermark(QString convId);
    void authenticationDone();
    void initDone();
    void networkReply();
    void postReply(QNetworkReply *reply);
    void sendMessageReply();
    void uploadImageReply();
    void uploadPerformedReply();
    void syncAllNewEventsReply();
    //void syncAllNewEventsDataArrval();
    void setActiveClientReply();
    void setTypingReply();
    void setPresenceReply();
    void updateWatermarkReply();
    void pvtReply();
    void channelLostSlot();
    void channelRestoredSlot(QDateTime lastRec);
    //void connectivityChanged(QString a,QDBusVariant b);
    void isTypingSlot(QString convId, QString chatId, int type);
    void authFailedSlot(QString error);
    void loginNeededSlot();
    void updateClientId(QString newID);
    void setFocusReply();
    void cookieUpdateSlot(QNetworkCookie cookie);
    void qnamUpdatedSlot(QNetworkAccessManager *qnam);

Q_SIGNALS:
    void loginNeeded();
    void messageSent();
    void messageNotSent();
    void conversationLoaded();
    void initFinished();
    void channelLost();
    void channelRestored();
    void isTyping(QString convid, QString uname, int status);
    void showNotification(QString preview, QString summary, QString body, QString sender);
    void authFailed(QString error);
    void incomingMessage(Event);

private:
    void sendImageMessage(QString convId, QString imgId, QString segments);
    void performImageUpload(QString url);
    void forceChannelCheckAndRestore();

    User parseMySelf(QString sreply);
    void getPVTToken();
    User getUserById(QString chatId);
    User parseEntity(QString input);
    QList<User> parseClientEntities(QString input);
    QList<User> parseGroup(QString input);
    QList<User> parseUsers(QString userString);

    QString getRequestHeader();
    Participant parseParticipant(QString plist);
    QList<Participant> parseParticipants(QString plist, QString data);
    Conversation parseConversationDetails(QString conversation, Conversation res);
    Conversation parseSelfConversationState(QString scState, Conversation res);
    Conversation parseConversationAbstract(QString conv, Conversation res);
    Conversation parseConversation(QString conv, int &start);
    void parseConversationState(QString conv);
    User parseUser(QString conv, int &startPos);
    QList<Conversation> parseConversations(QString conv);
    void followRedirection(QUrl url);
    int findPositionFromComma(QString input, int startPos, int commaCount);

    QByteArray getAuthHeader();
    QNetworkReply * sendRequest(QString function, QString json);
    void syncAllNewEvents(QDateTime timestamp);

    bool mAppPaused;
    bool mInitCompleted;
    bool mNeedSync;
    bool mNeedLogin;
    QNetworkAccessManager *mNetworkAccessManager;
    QDateTime mNeedSyncTS;
    QDateTime mLastSetActive;
    QList<OutgoingImage> mOutgoingImages;
    QString mCookiePath;
    Authenticator *mAuthenticator;
    QNetworkCookieJar mCookieJar;
    QList<QNetworkCookie> mSessionCookies;
    QString mApiKey, mHeaderDate, mHeaderVersion, mHeaderId, mChannelPath, mClid, mChannelEcParam, mChannelPropParam, mSyncTimestamp;
    User mMyself;
    Channel *mChannel;
    QList<User> mUsers;
    QMap<QString, Conversation> mConversations;
};

#endif // CLIENT_H
