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

#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QJsonArray>
#include <QUrlQuery>

#include "hangishclient.h"
#include "channel.h"

HangishClient::HangishClient(const QString &pCookiePath) :
    mCurrentRequestId(0),
    mNeedSync(false),
    mLastKnownPushTs(0),
    mCookiePath(pCookiePath),
    mAuthenticator(new Authenticator(mCookiePath)),
    mChannel(NULL)
{
    QObject::connect(mAuthenticator, SIGNAL(gotCookies(QMap<QString,QNetworkCookie>)), this, SLOT(onAuthenticationDone(QMap<QString,QNetworkCookie>)));
    QObject::connect(mAuthenticator, SIGNAL(loginNeeded()), this, SIGNAL(loginNeeded()));
    QObject::connect(mAuthenticator, SIGNAL(authFailed(AuthenticationStatus,QString)), this, SIGNAL(authFailed(AuthenticationStatus,QString)));
    qsrand((uint)QTime::currentTime().msec());
    mCurrentRequestId = qrand();
}

void HangishClient::initDone()
{
    if (mChannel) {
        hangishDisconnect();
    }
    mChannel = new Channel(mSessionCookies, mChannelPath, mHeaderId, mChannelEcParam, mChannelPropParam, mMyself);
    QObject::connect(mChannel, SIGNAL(statusChanged(Channel::ChannelStatus)), this, SLOT(onChannelStatusChanged(Channel::ChannelStatus)));
    QObject::connect(mChannel, SIGNAL(channelRestored(quint64)), this, SLOT(onChannelRestored(quint64)));
    QObject::connect(mChannel, SIGNAL(updateClientId(QString)), this, SLOT(updateClientId(QString)));
    QObject::connect(mChannel, SIGNAL(cookieUpdateNeeded(QNetworkCookie)), this, SLOT(cookieUpdateSlot(QNetworkCookie)));
    QObject::connect(mChannel, SIGNAL(clientBatchUpdate(ClientBatchUpdate&)), this, SLOT(onClientBatchUpdate(ClientBatchUpdate&)));

    syncAllNewEvents(mLastKnownPushTs);
    mChannel->listen();
}

void HangishClient::onChannelStatusChanged(Channel::ChannelStatus status)
{
    switch(status) {
    case Channel::ChannelStatusConnecting:
        Q_EMIT connectionStatusChanged(CONNECTION_STATUS_CONNECTING);
        break;
    case Channel::ChannelStatusPermanentError:
    case Channel::ChannelStatusInactive:
        Q_EMIT connectionStatusChanged(CONNECTION_STATUS_DISCONNECTED);
        break;
    }
}

void HangishClient::hangishDisconnect()
{
    QObject::disconnect(mChannel, 0, 0, 0);
    mChannel->deleteLater();
    mChannel = NULL;
    mClid = QString::null;
}

void HangishClient::hangishConnect(quint64 lastKnownPushTs)
{
    mNetworkAccessManager.setCookieJar(new QNetworkCookieJar(this));
    mSessionCookies.clear();
    mLastKnownPushTs = lastKnownPushTs;
    mAuthenticator->authenticate();
}

QString HangishClient::getSelfChatId() const
{
    if (mMyself.has_id() && mMyself.id().has_chatid()) {
        return mMyself.id().chatid().c_str();
    }
    return QString();
}

ClientEntity HangishClient::getMyself() const
{
    return mMyself;
}

QMap<QString, ClientEntity> HangishClient::getUsers() const
{
    return mUsers;
}

ClientEntity HangishClient::getUserById(const QString &chatId) const
{
    return mUsers[chatId];
}

ClientConversationState HangishClient::getConvById(const QString &convId) const
{
    return mConversations[convId];
}

QByteArray HangishClient::getAuthHeader() const
{
    QByteArray res = "SAPISIDHASH ";
    qint64 time_msec = QDateTime::currentMSecsSinceEpoch();//1000;
    QString auth_string = QString::number(time_msec);
    auth_string += " ";
    Q_FOREACH (QNetworkCookie cookie, mSessionCookies) {
        if (cookie.name()=="SAPISID") {
            auth_string += cookie.value();
        }
    }
    auth_string += " ";
    auth_string += ORIGIN_URL;
    res += QString::number(time_msec);
    res += "_";
    res += QCryptographicHash::hash(auth_string.toUtf8(), QCryptographicHash::Sha1).toHex();
    return res;
}

void HangishClient::performImageUpload(const QString &url)
{
    OutgoingImage oi = mOutgoingImages.at(0);

    QFile inFile(oi.filename);
    if (!inFile.open(QIODevice::ReadOnly)) {
        qDebug() << "File not found";
        return ;
    }

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", USER_AGENT);
    req.setRawHeader("X-GUploader-Client-Info", "mechanism=scotty xhr resumable; clientVersion=82480166");
    req.setRawHeader("content-type", "application/x-www-form-urlencoded;charset=utf-8");
    req.setRawHeader("Content-Length", QByteArray::number(inFile.size()));

    QList<QNetworkCookie> reqCookies;
    Q_FOREACH (QNetworkCookie cookie, mSessionCookies) {
        if (cookie.name()=="SAPISID" || cookie.name()=="SSID" || cookie.name()=="HSID" || cookie.name()=="APISID" || cookie.name()=="SID") {
            reqCookies.append(cookie);
        }
    }
    req.setHeader(QNetworkRequest::CookieHeader, QVariant::fromValue(reqCookies));
    QNetworkReply * reply = mNetworkAccessManager.post(req, inFile.readAll());
    QObject::connect(reply, SIGNAL(finished()), this, SLOT(uploadPerformedReply()));

}

void HangishClient::uploadPerformedReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());

    QVariant v = reply->header(QNetworkRequest::SetCookieHeader);
    QList<QNetworkCookie> c = qvariant_cast<QList<QNetworkCookie> >(v);
    qDebug() << "Got " << c.size() << "from" << reply->url();
    Q_FOREACH (QNetworkCookie cookie, c) {
        qDebug() << cookie.name();
    }
    QString sreply = reply->readAll();
    qDebug() << "Response " << sreply;
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200) {
        qDebug() << "Upload in progress";
        QJsonDocument doc = QJsonDocument::fromJson(sreply.toUtf8());
        QJsonObject obj = doc.object().take("sessionStatus").toObject();
        QString state = obj.take("state").toString();
        qDebug() << state;
        if (state=="FINALIZED") {
            //Completed, send the message!

            //Retrieve the id of the image
            QString imgId = obj.take("additionalInfo").toObject()
                            .take("uploader_service.GoogleRupioAdditionalInfo").toObject()
                            .take("completionInfo").toObject()
                            .take("customerSpecificInfo").toObject()
                            .take("photoid").toString();
            qDebug() << "Sending msg with img" << imgId;

            sendImageMessage(mOutgoingImages.at(0).conversationId, imgId, "");
            mOutgoingImages.clear();
        }
    } else {
        qDebug() << "Problem uploading";
    }
    reply->deleteLater();
}

QNetworkReply *HangishClient::sendRequest(const QString &function, const QString &json)
{
    QUrl url(ENDPOINT_URL + function);
    QUrlQuery query;
    query.addQueryItem("alt", "protojson");
    query.addQueryItem("key", mApiKey);
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setRawHeader("authorization", getAuthHeader());
    req.setRawHeader("x-origin", QByteArray(ORIGIN_URL));
    req.setRawHeader("x-goog-authuser", "0");
    req.setRawHeader("content-type", "application/json+protobuf");

    QList<QNetworkCookie> reqCookies;
    Q_FOREACH (QNetworkCookie cookie, mSessionCookies) {
        if (cookie.name()=="SAPISID" || cookie.name()=="SSID" || cookie.name()=="HSID" || cookie.name()=="APISID" || cookie.name()=="SID") {
            reqCookies.append(cookie);
        }
    }
    req.setHeader(QNetworkRequest::CookieHeader, QVariant::fromValue(reqCookies));
    QByteArray postData;
    postData.append(json);
    return mNetworkAccessManager.post(req, postData);
}

ClientRequestHeader *HangishClient::getRequestHeader1() const
{
    ClientRequestHeader *requestHeader =  new ClientRequestHeader;
    ClientClientVersion *clientVersion = new ClientClientVersion;
    ClientClientIdentifier *clientIdentifier = new ClientClientIdentifier;
    clientVersion->set_clientid(ClientClientVersion::QUASAR);
    clientVersion->set_buildtype(ClientClientVersion::PRODUCTION);
    clientVersion->set_majorversion(mHeaderVersion.toStdString());
    clientVersion->set_version(mHeaderDate.toLongLong());

    clientIdentifier->set_resource(mClid.toStdString());
    clientIdentifier->set_headerid(mHeaderId.toStdString());

    requestHeader->set_allocated_clientversion(clientVersion);
    requestHeader->set_allocated_clientidentifier(clientIdentifier);

    requestHeader->set_languagecode("en");

    return requestHeader;
}

QString HangishClient::getRequestHeader() const
{
    QString res = "[[3, 3, \"";
    res += mHeaderVersion;
    res += "\", \"";
    res += mHeaderDate;
    res += "\"], [\""+mClid+"\" ,\"";
    res += mHeaderId;
    res += "\"], null, \"en\"]";
    return res;
}

void HangishClient::sendImageMessage(const QString &convId, const QString &imgId, const QString &segments)
{
    QString seg = "[[0, \"";
    seg += segments;
    seg += "\", [0, 0, 0, 0], [null]]]";
    //qDebug() << "Sending cm " << segments;

    if (segments=="") {
        seg = "[]";
    }

    //Not really random, but works well
    qint64 time = QDateTime::currentMSecsSinceEpoch();
    uint random = (uint)(time % qint64(4294967295u));
    QString body = "[";
    body += getRequestHeader();
    //qDebug() << "gotH " << body;
    body += ", null, null, null, [], [";
    body += seg;
    body += ", []], [[\"";
    body += imgId;
    body += "\", false]], [[\"";
    body += convId;
    body += "\"], ";
    body += QString::number(random);
    body += ", 2, [1]], null, null, null, []]";
    //Eventually body += ", 2, [1]], ["chatId",null,null,null,null,[]], null, null, []]";
    qDebug() << "gotH " << body;
    QNetworkReply *reply = sendRequest("conversations/sendchatmessage",body);
    QObject::connect(reply, SIGNAL(finished()), this, SLOT(sendMessageReply()));
}

quint64 HangishClient::sendChatMessage(ClientSendChatMessageRequest clientSendChatMessageRequest)
{
    quint64 requestId = mCurrentRequestId++;
    clientSendChatMessageRequest.set_allocated_requestheader(getRequestHeader1());

    QNetworkReply *reply = sendRequest("conversations/sendchatmessage", Utils::msgToJsArray(clientSendChatMessageRequest));
    QObject::connect(reply, SIGNAL(finished()), this, SLOT(sendMessageReply()));
    mPendingRequests[reply] = requestId;
    return requestId;
}

void HangishClient::sendMessageReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());

    quint64 requestId = mPendingRequests.take(reply);

    QVariant v = reply->header(QNetworkRequest::SetCookieHeader);
    QList<QNetworkCookie> c = qvariant_cast<QList<QNetworkCookie> >(v);
    qDebug() << "Got " << c.size() << "from" << reply->url();
    Q_FOREACH (QNetworkCookie cookie, c) {
        qDebug() << cookie.name();
    }
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200) {
        qDebug() << "Message sent correctly: " << requestId;
        Q_EMIT messageSent(requestId);
    } else {
        qDebug() << "Failed to send message: " << requestId;
        Q_EMIT messageNotSent(requestId);
    }
    delete reply;
}

void HangishClient::sendImage(const QString &segments, const QString &conversationId, const QString &filename)
{
    Q_UNUSED(segments)
    QFile inFile(filename);
    if (!inFile.open(QIODevice::ReadOnly)) {
        qDebug() << "File not found";
        return ;
    }

    OutgoingImage oi;
    oi.conversationId = conversationId;
    oi.filename = filename;
    mOutgoingImages.append(oi);

    //First upload image to gdocs
    QJsonObject emptyObj;
    QJsonArray jarr;
    QJsonObject j1, jj1;
    j1["name"] = QJsonValue(QString("file"));
    j1["filename"] = QJsonValue(QString(filename.right(filename.size()-filename.lastIndexOf("/")-1)));
    j1["put"] = emptyObj;
    j1["size"] =  QJsonValue(inFile.size());
    jj1["external"] = j1;

    QJsonObject j2, jj2;
    j2["name"] = QJsonValue(QString("album_mode"));
    j2["content"] = QJsonValue(QString("temporary"));
    j2["contentType"] = QJsonValue(QString("text/plain"));
    jj2["inlined"] = j2;

    QJsonObject j3, jj3;
    j3["name"] = QJsonValue(QString("title"));
    j3["content"] = QJsonValue(QString(filename.right(filename.size()-filename.lastIndexOf("/")-1)));
    j3["contentType"] = QJsonValue(QString("text/plain"));
    jj3["inlined"] = j3;

    qint64 time = QDateTime::currentMSecsSinceEpoch();
    QJsonObject j4, jj4;
    j4["name"] = QJsonValue(QString("addtime"));
    j4["content"] = QJsonValue(QString::number(time));
    j4["contentType"] = QJsonValue(QString("text/plain"));
    jj4["inlined"] = j4;

    QJsonObject j5, jj5;
    j5["name"] = QJsonValue(QString("batchid"));
    j5["content"] = QJsonValue(QString::number(time));
    j5["contentType"] = QJsonValue(QString("text/plain"));
    jj5["inlined"] = j5;

    QJsonObject j6, jj6;
    j6["name"] = QJsonValue(QString("album_name"));
    j6["content"] = QJsonValue(QString("hangish"));
    j6["contentType"] = QJsonValue(QString("text/plain"));
    jj6["inlined"] = j6;

    QJsonObject j7, jj7;
    j7["name"] = QJsonValue(QString("album_abs_position"));
    j7["content"] = QJsonValue(QString("0"));
    j7["contentType"] = QJsonValue(QString("text/plain"));
    jj7["inlined"] = j7;

    QJsonObject j8, jj8;
    j8["name"] = QJsonValue(QString("client"));
    j8["content"] = QJsonValue(QString("hangouts"));
    j8["contentType"] = QJsonValue(QString("text/plain"));
    jj8["inlined"] = j8;

    jarr.append(jj1);
    jarr.append(jj2);
    jarr.append(jj3);
    jarr.append(jj4);
    jarr.append(jj5);
    jarr.append(jj6);
    jarr.append(jj7);
    jarr.append(jj8);

    QJsonObject jjson;
    jjson["fields"] = jarr;

    QJsonObject json;
    json["createSessionRequest"] = jjson;
    json["protocolVersion"] = QJsonValue(QString("0.8"));


    //url += "?alt=json&key=";
    //url += mApiKey;
    qDebug() << "Sending request for up image";
    QJsonDocument doc(json);
    qDebug() << doc.toJson();

    QNetworkRequest req(QUrl("https://docs.google.com/upload/photos/resumable?authuser=0"));
    //req.setRawHeader("authorization", getAuthHeader());
    req.setRawHeader("User-Agent", USER_AGENT);
    req.setRawHeader("X-GUploader-Client-Info", "mechanism=scotty xhr resumable; clientVersion=82480166");
    req.setRawHeader("content-type", "application/x-www-form-urlencoded;charset=utf-8");
    req.setRawHeader("Content-Length", QByteArray::number(doc.toJson().size()));

    QList<QNetworkCookie> reqCookies;
    Q_FOREACH (QNetworkCookie cookie, mSessionCookies) {
        if (cookie.name()=="SAPISID" || cookie.name()=="SSID" || cookie.name()=="HSID" || cookie.name()=="APISID" || cookie.name()=="SID") {
            reqCookies.append(cookie);
        }
    }
    req.setHeader(QNetworkRequest::CookieHeader, QVariant::fromValue(reqCookies));
    QNetworkReply * reply = mNetworkAccessManager.post(req, doc.toJson());
    QObject::connect(reply, SIGNAL(finished()), this, SLOT(uploadImageReply()));
    //Then send the message
}

void HangishClient::uploadImageReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    qDebug() << "DbG: " << reply;

    QVariant v = reply->header(QNetworkRequest::SetCookieHeader);
    QList<QNetworkCookie> c = qvariant_cast<QList<QNetworkCookie> >(v);
    qDebug() << "Got " << c.size() << "from" << reply->url();
    Q_FOREACH (QNetworkCookie cookie, c) {
        qDebug() << cookie.name();
    }
    QString sreply = reply->readAll();
    qDebug() << "Response " << sreply;
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200) {
        qDebug() << "Upload ready";
        QVariant v = reply->header(QNetworkRequest::LocationHeader);
        QString uploadUrl = qvariant_cast<QString>(v);
        qDebug() << uploadUrl;
        reply->close();
        performImageUpload(uploadUrl);
    } else {
        qDebug() << "Problem uploading " << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }
    reply->deleteLater();
}

quint64 HangishClient::queryPresence(const QStringList &chatIds)
{
    quint64 requestId = mCurrentRequestId++;
    ClientQueryPresenceRequest clientQueryPresenceRequest;
    ClientParticipantList *participantList = new ClientParticipantList();
    ClientFieldMaskList *fieldMaskList = new ClientFieldMaskList();
    clientQueryPresenceRequest.set_allocated_requestheader(getRequestHeader1());
    clientQueryPresenceRequest.set_allocated_participantlist(participantList);
    clientQueryPresenceRequest.set_allocated_fieldmasklist(fieldMaskList);
    Q_FOREACH(const QString &chatId, chatIds) {
        ClientParticipantId *participantId = participantList->add_participantid();
        participantId->set_gaiaid(chatId.toLocal8Bit().data());
    }
    fieldMaskList->add_fieldmask(ClientQueryPresenceRequest_ClientFieldMask_AVAILABILITY);
    fieldMaskList-> add_fieldmask(ClientQueryPresenceRequest_ClientFieldMask_STATUS_MESSAGE);
    QNetworkReply *reply = sendRequest("presence/querypresence", Utils::msgToJsArray(clientQueryPresenceRequest));
    mPendingRequests[reply] = requestId;
    QObject::connect(reply, SIGNAL(finished()), this, SLOT(queryPresenceReply()));
    return requestId;
}

void HangishClient::queryPresenceReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QString sreply = reply->readAll();
    ClientQueryPresenceResponse cqprp;
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200) {
        qDebug() << "There was an error getting presence! " << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        return;
    }
    QVariantList variantListResponse = Utils::jsArrayToVariantList(sreply);
    if (variantListResponse[0].toString() == "cqprp") {
        variantListResponse.pop_front();
        Utils::packToMessage(QVariantList() << variantListResponse, cqprp);
        Q_EMIT clientQueryPresenceResponse(mPendingRequests.take(reply), cqprp);
    }
}

quint64 HangishClient::setPresence(bool goingOnline)
{
    quint64 requestId = mCurrentRequestId++;
    ClientSetPresenceRequest clientSetPresenceRequest;
    clientSetPresenceRequest.set_allocated_requestheader(getRequestHeader1());
    ClientPresenceStateSetting *clientPresenceStateSetting = new ClientPresenceStateSetting();
    clientPresenceStateSetting->set_timeoutsecs(720);
    clientPresenceStateSetting->set_presencestate(goingOnline ? ClientPresenceStateSetting_ClientPresenceState_DESKTOP_ACTIVE : ClientPresenceStateSetting_ClientPresenceState_MOBILE);
    clientSetPresenceRequest.set_allocated_presencestatesetting(clientPresenceStateSetting);

    QNetworkReply *reply = sendRequest("presence/setpresence", Utils::msgToJsArray(clientSetPresenceRequest));
    mPendingRequests[reply] = requestId;
    QObject::connect(reply, SIGNAL(finished()), this, SLOT(setPresenceReply()));
    return requestId;
}

void HangishClient::setPresenceReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QString sreply = reply->readAll();
    qDebug() << "Set presence response " << sreply;
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200) {
        qDebug() << "There was an error setting presence! " << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }
    ClientSetPresenceResponse csprp;
    QVariantList variantListResponse = Utils::jsArrayToVariantList(sreply);
    if (variantListResponse[0].toString() == "csprp") {
        variantListResponse.pop_front();
        Utils::packToMessage(QVariantList() << variantListResponse, csprp);
        Q_EMIT clientSetPresenceResponse(mPendingRequests.take(reply), csprp);
    }
}

void HangishClient::setFocus(const QString &convId, int status)
{
    QString body = "[";
    body += getRequestHeader();
    body += ", [\"";
    body += convId;
    body += "\"], ";
    body += QString::number(status);
    body += ", 20]";
    qDebug() << body;
    QNetworkReply *reply = sendRequest("conversations/setfocus",body);
    QObject::connect(reply, SIGNAL(finished()), this, SLOT(setFocusReply()));
}

void HangishClient::setFocusReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QString sreply = reply->readAll();
    qDebug() << "Set focus response " << sreply;
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200) {
        qDebug() << "There was an error setting focus! " << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }
}

void HangishClient::setTyping(const QString &convId, int status)
{
    ClientSetTypingRequest request;
    //ClientConversationId conversationId = new ClientConversationId();
    QString body = "[";
    body += getRequestHeader();
    body += ", [\"";
    body += convId;
    body += "\"], ";
    body += QString::number(status);
    body += "]";
    qDebug() << body;
    QNetworkReply *reply = sendRequest("conversations/settyping",body);
    QObject::connect(reply, SIGNAL(finished()), this, SLOT(setTypingReply()));
}

void HangishClient::setTypingReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QString sreply = reply->readAll();
    qDebug() << "Set typing response " << sreply;
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200) {
        qDebug() << "There was an error setting typing status! " << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }
}

quint64 HangishClient::getConversation(ClientGetConversationRequest clientGetConversationRequest)
{
    quint64 requestId = mCurrentRequestId++;
    clientGetConversationRequest.set_allocated_requestheader(getRequestHeader1());
    QNetworkReply *reply = sendRequest("conversations/getconversation", Utils::msgToJsArray(clientGetConversationRequest));
    QObject::connect(reply, SIGNAL(finished()), this, SLOT(getConversationReply()));
    mPendingRequests[reply] = requestId;
    return requestId;
}

void HangishClient::getConversationReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());

    QVariant v = reply->header(QNetworkRequest::SetCookieHeader);
    QList<QNetworkCookie> c = qvariant_cast<QList<QNetworkCookie> >(v);
    qDebug() << "Got " << c.size() << "from" << reply->url();
    Q_FOREACH (QNetworkCookie cookie, c) {
        qDebug() << cookie.name();
    }
    QString sreply = reply->readAll();

    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200) {

        ClientGetConversationResponse cgcr;
        QVariantList variantListResponse = Utils::jsArrayToVariantList(sreply);
        if (variantListResponse[0].toString() == "cgcrp") {
            variantListResponse.pop_front();
            Utils::packToMessage(QVariantList() << variantListResponse, cgcr);
            Q_EMIT clientGetConversationResponse(mPendingRequests.take(reply), cgcr);
        }
    }
    reply->deleteLater();
}

void HangishClient::syncAllNewEvents(quint64 timestamp)
{
    ClientSyncAllNewEventsRequest clientSyncAllNewEventsRequest;
    clientSyncAllNewEventsRequest.set_allocated_requestheader(getRequestHeader1());
    clientSyncAllNewEventsRequest.set_lastsynctimestamp(timestamp);
    clientSyncAllNewEventsRequest.set_nomissedeventsexpected(false);
    clientSyncAllNewEventsRequest.set_maxresponsesizebytes(1048576);

    QNetworkReply *reply = sendRequest("conversations/syncallnewevents", Utils::msgToJsArray(clientSyncAllNewEventsRequest));
    QObject::connect(reply, SIGNAL(finished()), this, SLOT(syncAllNewEventsReply()));
}

void HangishClient::syncAllNewEventsReply()
{
    //The content of this reply contains CLIENT_CONVERSATION_STATE, such as lost messages
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());

    QVariant v = reply->header(QNetworkRequest::SetCookieHeader);
    QList<QNetworkCookie> c = qvariant_cast<QList<QNetworkCookie> >(v);
    qDebug() << "Got " << c.size() << "from" << reply->url();
    Q_FOREACH (QNetworkCookie cookie, c) {
        qDebug() << cookie.name();
    }
    QString sreply = reply->readAll();

    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200) {
        qDebug() << "Synced correctly";

        ClientSyncAllNewEventsResponse csanerp;
        QVariantList variantListResponse = Utils::jsArrayToVariantList(sreply);
        if (variantListResponse[0].toString() == "csanerp") {
            variantListResponse.pop_front();
            Utils::packToMessage(QVariantList() << variantListResponse, csanerp);
            Q_EMIT clientSyncAllNewEventsResponse(csanerp);
        }
        mNeedSync = false;
    }
    reply->deleteLater();
}

void HangishClient::setActiveClient()
{
    QDateTime now = QDateTime::currentDateTime();
    if (mLastSetActive.addSecs(SETACTIVECLIENT_LIMIT_SECS) > now) {
        return;
    }
    mLastSetActive = now;
    QString body = "[";
    body += getRequestHeader();
    body += ", " + QString::number(IS_ACTIVE_CLIENT) +" , \"";
    body += mMyself.properties().email(0).c_str();
    body += "/";
    body += mClid;
    body += "\", ";
    body += QString::number(ACTIVE_TIMEOUT_SECS);
    body += "]";
    qDebug() << body;
    QNetworkReply *reply = sendRequest("clients/setactiveclient",body);
    QObject::connect(reply, SIGNAL(finished()), this, SLOT(setActiveClientReply()));
}

void HangishClient::setActiveClientReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QString sreply = reply->readAll();
    qDebug() << "Set active client response " << sreply;
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200) {
        qDebug() << "There was an error setting active client! " << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    } else {
        //I've just set this; I can assume I am the active client
        //notifier->activeClientUpdate(IS_ACTIVE_CLIENT);
    }
}

void HangishClient::updateWatermark(QString convId)
{
    qDebug() << "Updating wm";
    //If there are no unread messages we can avoid generating data traffic
    //if (!rosterModel->hasUnreadMessages(convId))
    //    return;
    QString body = "[";
    body += getRequestHeader();
    body += ", [";
    body += convId;
    body += "], ";
    body += QString::number(QDateTime::currentDateTime().toMSecsSinceEpoch()*1000);
    body += "]";
    qDebug() << body;
    QNetworkReply *reply = sendRequest("conversations/updatewatermark",body);
    QObject::connect(reply, SIGNAL(finished()), this, SLOT(updateWatermarkReply()));
}

void HangishClient::updateWatermarkReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());

    QString sreply = reply->readAll();
    qDebug() << "Update watermark response " << sreply;
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200) {
        qDebug() << "There was an error updating the wm! " << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }
}

void HangishClient::initChat(const QString &pvt)
{
    QUrlQuery query;

    query.addQueryItem("prop", "hangish");
    query.addQueryItem("fid", "gtn-roster-iframe-id");
    query.addQueryItem("ec", "[\"ci:ec\",true,true,false]");
    query.addQueryItem("pvt", pvt);

    QUrl url(CHAT_INIT_URL);
    url.setQuery(query);
    QNetworkRequest req( url );
    req.setRawHeader("User-Agent", USER_AGENT);
    req.setHeader(QNetworkRequest::CookieHeader, QVariant::fromValue(mSessionCookies.values()));
    QNetworkReply * reply = mNetworkAccessManager.get(req);
    QObject::connect(reply, SIGNAL(finished()), this, SLOT(onInitChatReply()));
}

void HangishClient::followRedirection(const QUrl &url)
{
    QNetworkRequest req( url );
    req.setRawHeader("User-Agent", USER_AGENT);
    req.setHeader(QNetworkRequest::CookieHeader, QVariant::fromValue(mSessionCookies.values()));
    QNetworkReply * reply = mNetworkAccessManager.get(req);
    QObject::connect(reply, SIGNAL(finished()), this, SLOT(onInitChatReply()));
}

void HangishClient::onInitChatReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QVariant v = reply->header(QNetworkRequest::SetCookieHeader);
    QList<QNetworkCookie> c = qvariant_cast<QList<QNetworkCookie> >(v);
    qDebug() << "DBG Got " << c.size() << "from" << reply->url();
    Q_FOREACH (QNetworkCookie cookie, c) {
        if (mSessionCookies.contains(cookie.name())) {
            qDebug() << "Updating cookie " << cookie.name();
            mSessionCookies[cookie.name()] = cookie;
        }
    }

    if (reply->error() == QNetworkReply::NoError) {
        qDebug() << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==302) {
            qDebug() << "Redir";
            QVariant possibleRedirectUrl =
                reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
            followRedirection(possibleRedirectUrl.toUrl());
            reply->deleteLater();
        } else {
            QString sreply = reply->readAll();

            QRegExp rx("(\\[\\[\"cin:cac\".*\\}\\}\\)\\;)");
            if (rx.indexIn(sreply) != -1) {
                QString cincac = rx.cap(1).split("}});")[0];
                QVariantList list = Utils::jsArrayToVariantList(cincac)[0].toList();
                if (list[0] =="cin:cac") {
                    list.removeAt(0);
                    ChatApiConfiguration chatApiConfiguration;
                    Utils::packToMessage(list, chatApiConfiguration);
                    mApiKey = chatApiConfiguration.key().c_str();
                }
            }
            if (mApiKey.isEmpty()) {
                qDebug() << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                qDebug() << "Auth expired!";
                //Not smart for sure, but it should be safe
                deleteCookies();
            }

            rx.setPattern("(\\[\\[\"cin:bcsc\".*\\}\\}\\)\\;)");
            if (rx.indexIn(sreply) != -1) {
                QString cinbcsc = rx.cap(1).split("}});")[0];
                QVariantList list = Utils::jsArrayToVariantList(cinbcsc)[0].toList();
                if (list[0] =="cin:bcsc") {
                    list.removeAt(0);
                    EcConfiguration ecConfiguration;
                    Utils::packToMessage(list, ecConfiguration);
                    mChannelPath = ecConfiguration.channelpath().c_str();
                    mChannelEcParam = ecConfiguration.ecparam().c_str();
                    mChannelPropParam = ecConfiguration.propparam().c_str();
                    mHeaderId = ecConfiguration.headerid().c_str();
                }
            }

            QString cinaccReply;
            int cinaccReplyPos = 0;
            if ((cinaccReplyPos = sreply.indexOf("key: 'ds:2'")) != -1) {
                cinaccReply = sreply.mid(cinaccReplyPos);
            }
            rx.setPattern("(\\[\\[\"cin:acc\".*\\}\\}\\)\\;)");
            if (rx.indexIn(cinaccReply) != -1) {
                QString cinacc = rx.cap(1).split("}});")[0];
                QVariantList list = Utils::jsArrayToVariantList(cinacc)[0].toList();
                if (list[0] =="cin:acc") {
                    list.removeAt(0);
                    ChatInitParameters chatInitParameters;
                    Utils::packToMessage(list, chatInitParameters);
                    mHeaderDate = chatInitParameters.headerdate().c_str();
                    mHeaderVersion = chatInitParameters.headerversion().c_str();
                }
            }

            qDebug() << "HID " << mHeaderId;
            qDebug() << "HDA " << mHeaderDate;
            qDebug() << "HVE " << mHeaderVersion;

            // Parse myself
            rx.setPattern("(\\[\\[\"cgsirp\".*\\}\\}\\)\\;)");
            if (rx.indexIn(sreply) != -1) {
                QString cgsirp = rx.cap(1).split("}});")[0];
                QVariantList list = Utils::jsArrayToVariantList(cgsirp)[0].toList();
                if (list[0] =="cgsirp") {
                    list.removeAt(0);
                    ClientGetSelfInfoResponse clientGetSelfInfoResponse;
                    Utils::packToMessage(list, clientGetSelfInfoResponse);
                    mMyself = clientGetSelfInfoResponse.selfentity();
                }
            }

            // Parse Users
            rx.setPattern("(\\[\\[\"cgserp\".*\\}\\}\\)\\;)");
            if (rx.indexIn(sreply) != -1) {
                QString cgserp = rx.cap(1).split("}});")[0];
                QVariantList list = Utils::jsArrayToVariantList(cgserp)[0].toList();
                if (list[0] =="cgserp") {
                    list.removeAt(0);
                    ClientGetSuggestedEntitiesResponse clientGetSuggestedEntitiesResponse;
                    Utils::packToMessage(list, clientGetSuggestedEntitiesResponse);
                    if (clientGetSuggestedEntitiesResponse.has_hangoutcontacts()) {
                        ClientContactGroup contactGroup = clientGetSuggestedEntitiesResponse.hangoutcontacts();
                        for (int i =0; i < contactGroup.contactentity_size(); i++) {
                            ClientContactEntity contactEntity = contactGroup.contactentity(i);
                            ClientEntity entity = contactEntity.entity();
                            mUsers[QString(entity.id().chatid().c_str())] = entity;
                        }
                    }
                }
            }

            //Parse conversations
            rx.setPattern("(\\[\\[\"csrcrp\".*\\}\\}\\)\\;)");
            if (rx.indexIn(sreply) != -1) {
                QString csrcrp = rx.cap(1).split("}});")[0];
                QVariantList list = Utils::jsArrayToVariantList(csrcrp)[0].toList();
                if (list[0] =="csrcrp") {
                    list.removeAt(0);
                    ClientSyncRecentConversationsResponse clientSyncRecentConversationsResponse;
                    Utils::packToMessage(list, clientSyncRecentConversationsResponse);
                    for (int i=0; i < clientSyncRecentConversationsResponse.conversationstate_size(); i++) {
                        ClientConversationState conv = clientSyncRecentConversationsResponse.conversationstate(i);
                        mConversations[QString(conv.conversationid().id().c_str())] = conv;
                    }
                }

            }
            reply->deleteLater();
            initDone();
        }
    } else {
        //failure
        //qDebug() << "Failure" << reply->errorString();
        reply->deleteLater();
    }
}

void HangishClient::onAuthenticationDone(QMap<QString, QNetworkCookie> cookies)
{
    mSessionCookies = cookies;
    mNetworkAccessManager.setCookieJar(new QNetworkCookieJar(this));
    getPVTToken();
}

void HangishClient::getPVTToken()
{
    QNetworkRequest req( QUrl( QString("https://talkgadget.google.com/talkgadget/_/extension-start") ) );
    req.setRawHeader("User-Agent", USER_AGENT);

    if (!mSessionCookies.isEmpty()) {
        req.setHeader(QNetworkRequest::CookieHeader, QVariant::fromValue(mSessionCookies.values()));
    }
    QNetworkReply * reply = mNetworkAccessManager.get(req);
    QObject::connect(reply, SIGNAL(finished()), this, SLOT(onGetPVTTokenReply()));
}

void HangishClient::onGetPVTTokenReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200) {
        QVariant v = reply->header(QNetworkRequest::SetCookieHeader);
        QList<QNetworkCookie> c = qvariant_cast<QList<QNetworkCookie> >(v);
        qDebug() << "Got " << c.size() << "cookies from" << reply->url();

        Q_FOREACH (QNetworkCookie cookie, c) {
            mSessionCookies[cookie.name()] = cookie;
        }

        if (c.size() > 0) {
            mAuthenticator->updateCookieFile(mSessionCookies.values());
        }

        PVTToken pvttoken;
        QVariantList variantListResponse = Utils::jsArrayToVariantList(reply->readAll());
        Utils::packToMessage(QVariantList() << variantListResponse, pvttoken);

        reply->close();

        if (!pvttoken.has_token()) {
            mAuthenticator->getGalxToken();
        } else {
            mNetworkAccessManager.setCookieJar(new QNetworkCookieJar(this));
            initChat(pvttoken.token().c_str());
        }
    } else {
        qDebug() << "Pvt req returned " << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->close();
    }
    reply->deleteLater();
}

void HangishClient::cookieUpdateSlot(QNetworkCookie cookie)
{
    qDebug() << "CLT: upd " << cookie.name();
    if (mSessionCookies.contains(cookie.name())) {
        mSessionCookies[cookie.name()] = cookie;
    }
    mNetworkAccessManager.setCookieJar(new QNetworkCookieJar(this));
}

void HangishClient::onClientBatchUpdate(ClientBatchUpdate &cbu)
{
    for (int i = 0; i < cbu.stateupdate_size(); i++) {
        ClientStateUpdate update = cbu.stateupdate(i);
        Q_EMIT clientStateUpdate(update);
    }
}

void HangishClient::sendCredentials(const QString &uname, const QString &passwd)
{
    mAuthenticator->sendCredentials(uname, passwd);
}

void HangishClient::sendChallengePin(const QString &pin)
{
    mAuthenticator->sendChallengePin(pin);
}

void HangishClient::deleteCookies()
{
    QFile cookieFile(mCookiePath);
    cookieFile.remove();
    exit(0);
}

void HangishClient::onChannelRestored(quint64 lastRec)
{
    //If there was another pending req use its ts (that should be older)
    if (!mNeedSync) {
        mNeedSync = true;
        mNeedSyncTS = lastRec;
    }
    qDebug() << "Channel restored, gonna sync with " << lastRec;
    syncAllNewEvents(mNeedSyncTS);
    Q_EMIT channelRestored();
}

void HangishClient::updateClientId(QString newID)
{
    qDebug() << "Updating mClid " << newID << sender();
    bool initDone = mClid.isEmpty();
    mClid = newID;
    if (initDone) {
        Q_EMIT initFinished();
    }
}
