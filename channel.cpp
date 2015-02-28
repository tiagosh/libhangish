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

#include "channel.h"

static QString user_agent = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/39.0.2171.71 Safari/537.36";
static qint32 MAX_READ_BYTES = 1024 * 1024;

Channel::Channel(QNetworkAccessManager *n, QList<QNetworkCookie> cookies, QString ppath, QString pclid, QString pec, QString pprop, User pms) :
    mLongPoolRequest(NULL),
    mChannelError(false),
    mNetworkAccessManager(n),
    mMyself(pms),
    mSessionCookies(cookies),
    mClid(pclid),
    mEc(pec),
    mPath(ppath),
    mProp(pprop),
    mLastPushReceived(QDateTime::currentDateTime())
{
    QTimer *timer = new QTimer(this);
    QObject::connect(timer, SIGNAL(timeout()), this, SLOT(checkChannel()));
    timer->start(30000);
}

ChannelEvent Channel::parseTypingNotification(QString input, ChannelEvent evt)
{
    int start = 1;
    QString conversationId = Utils::getNextAtomicField(input, start);
    conversationId = conversationId.mid(1, conversationId.size()-2);
    if (conversationId.isEmpty())
        return evt;

    QString userId = Utils::getNextAtomicField(input, start);
    if (userId.isEmpty())
        return evt;

    QString ts = Utils::getNextAtomicField(input, start);
    if (ts.isEmpty())
        return evt;

    QString typingStatus = input.mid(start, 1);
    evt.conversationId = conversationId.mid(1, conversationId.size()-2);
    evt.userId = userId;
    evt.typingStatus = typingStatus.toInt();

    qDebug() << "User " << userId << " in conv " << conversationId << " is typing " << typingStatus << " at " << ts;
    return evt;
}

void Channel::checkChannel()
{
    qDebug() << "Checking chnl";
    if (mLastPushReceived.secsTo(QDateTime::currentDateTime()) > 30) {
        qDebug() << "Dead, here I should sync al evts from last ts and notify QML that we're offline";
        qDebug() << "start new lpconn";
        mChannelError = true;
        Q_EMIT channelLost();
        QTimer::singleShot(500, this, SLOT(longPollRequest()));
    }
}

void Channel::fastReconnect()
{
    qDebug() << "fast reconnecting";
    QTimer::singleShot(500, this, SLOT(longPollRequest()));
}

void Channel::networkRequestFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QVariant v = reply->header(QNetworkRequest::SetCookieHeader);
    QList<QNetworkCookie> c = qvariant_cast<QList<QNetworkCookie> >(v);
    //Eventually update cookies, may set S
    bool cookieUpdated = false;
    Q_FOREACH (QNetworkCookie cookie, c) {
        qDebug() << cookie.name();
        for (int i=0; i<mSessionCookies.size(); i++) {
            if (mSessionCookies[i].name() == cookie.name()) {
                mSessionCookies[i].setValue(cookie.value());
                Q_EMIT cookieUpdateNeeded(cookie);
                qDebug() << "Updated cookie " << cookie.name();
                cookieUpdated = true;
            }
        }
    }
    if (cookieUpdated) {
        mNetworkAccessManager = new QNetworkAccessManager();
        Q_EMIT qnamUpdated(mNetworkAccessManager);
    }
    qDebug() << "FINISHED called! " << mChannelError;
    QString srep = reply->readAll();
    qDebug() << srep;
    if (srep.contains("Unknown SID")) {
        //Need new SID
        fetchNewSid();
        return;
    }
    //If there's a network problem don't do anything, the connection will be retried by checkChannelStatus
    if (!mChannelError) longPollRequest();
}

void Channel::parseChannelData(QString sreply)
{
    qDebug() << sreply;
    sreply = sreply.remove("\\n");
    sreply = sreply.remove("\n");
    sreply = sreply.remove(QChar('\\'));
    QString parcel;
    int parcelCursor = 0;
    for (;;) {
        //We skip the int representing the (Javascript) len of the parcel
        parcelCursor = sreply.indexOf("[", parcelCursor);
        if (parcelCursor==-1)
            return;
        parcel = Utils::getNextAtomicFieldForPush(sreply, parcelCursor);
        ////qDebug() << "PARCEL: " << parcel;
        if (parcel.size() < 20)
            return;

        int start = parcel.indexOf("\"c\"");
        if (start==-1)
            return;
        QString payload = Utils::getNextField(parcel, start + 4);
        //skip id
        start = 1;
        Utils::getNextAtomicField(payload, start);
        //and now the actual payload
        payload = Utils::getNextAtomicField(payload, start);
        QString type;
        start = 1;
        type = Utils::getNextAtomicField(payload, start);
        ////qDebug() << type;
        if (type != "\"bfo\"")
            continue;
        //and now the actual payload
        payload = Utils::getNextAtomicField(payload, start);
        //////qDebug() << "pld1: " << payload;

        start = 2;
        type = Utils::getNextAtomicField(payload, start);
        ////qDebug() << type;
        if (type != "\"cbu\"")
            continue;

        //This, finally, is the real payload
        int globalptr = 1;
        QString arr_payload = Utils::getNextField(payload, start);
        //////qDebug() << "arr pld: " << arr_payload;
        //But there could be more than 1
        for (;;) {

            payload = Utils::getNextAtomicField(arr_payload, globalptr);
            ////qDebug() << "pld: " << payload;
            if (payload.size() < 30) break;
            ChannelEvent cevt;
            start = 1;
            //Info about active clients -- if I'm not the active client then I must disable notifications!
            QString header = Utils::getNextAtomicField(payload, start);
            qDebug() << header;
            if (header.size()>10)
            {
                QString newId;
                int as = Utils::parseActiveClientUpdate(header, newId);
                Q_EMIT activeClientUpdate(as);
            }
            //conv notification; always none?
            Utils::getNextAtomicField(payload, start);
            //evt notification -- this holds the actual message
            QString evtstring = Utils::getNextAtomicFieldForPush(payload, start);
            qDebug() << evtstring;
            if (evtstring.size() > 10) {
                //evtstring has [evt, None], so we catch only the first value
                Event evt = Utils::parseEvent(Utils::getNextField(evtstring, 1));
                if (evt.value.valid) {
                    qDebug() << evt.sender.chat_id << " sent " << evt.value.segments[0].value;
                    //conversationModel->addEventToConversation(evt.conversationId, evt);
                    //if (evt.sender.chat_id != myself.chat_id) {
                    qDebug() << "Message received " << evt.value.segments[0].value;
                    Q_EMIT incomingMessage(evt);
                    //Signal new event only if the actual conversation isn't already visible to the user
                    //qDebug() << conversationModel->getCid();
                    qDebug() << evt.conversationId;
                    qDebug() << evt.notificationLevel;
                    /*if (evt.notificationLevel==30 && (appPaused || (conversationModel->getCid() != evt.conversationId))) {
                            rosterModel->addUnreadMsg(evt.conversationId);
                            Q_EMIT showNotification(evt.value.segments[0].value, evt.sender.chat_id, evt.value.segments[0].value, evt.sender.chat_id);
                        }
                        else {
                            //Update watermark, since I've read the message; if notification level this should be the active client
                            if (evt.notificationLevel==30)
                                Q_EMIT updateWM(evt.conversationId);
                        }*/
                    //}
                }
                else {
                    qDebug() << "Invalid evt received";
                }
            }
            //set focus notification
            Utils::getNextAtomicField(payload, start);
            //set typing notification
            QString typing = Utils::getNextAtomicFieldForPush(payload, start);
            qDebug() << "typing string " << typing;
            if (typing.size() > 3) {
                cevt = parseTypingNotification(typing, cevt);
                //I would know wether myself is typing :)
                if (!cevt.userId.contains(mMyself.chat_id))
                    Q_EMIT isTyping(cevt.conversationId, cevt.userId, cevt.typingStatus);
            }
            //notification level; wasn't this already in the event info?
            QString notifLev = Utils::getNextAtomicField(payload, start);
            qDebug() << notifLev;
            //reply to invite
            Utils::getNextAtomicField(payload, start);
            //watermark
            QString wmNotification = Utils::getNextAtomicField(payload, start);
            qDebug() << wmNotification;
            if (wmNotification.size()>10) {
                //conversationModel->updateReadState(Utils::parseReadStateNotification(wmNotification));
            }
            //None?
            Utils::getNextAtomicField(payload, start);
            //settings
            Utils::getNextAtomicField(payload, start);
            //view modification
            Utils::getNextAtomicField(payload, start);
            //easter egg
            Utils::getNextAtomicField(payload, start);
            //conversation
            QString conversation = Utils::getNextAtomicField(payload, start);
            ////qDebug() << "CONV: " << conversation;

            //self presence
            Utils::getNextAtomicField(payload, start);
            //delete notification
            Utils::getNextAtomicField(payload, start);
            //presence notification
            Utils::getNextAtomicField(payload, start);
            //block notification
            Utils::getNextAtomicField(payload, start);
            //invitation notification
            Utils::getNextAtomicField(payload, start);
        }
    }
}

void Channel::networReadyRead()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QVariant v = reply->header(QNetworkRequest::SetCookieHeader);
    QList<QNetworkCookie> c = qvariant_cast<QList<QNetworkCookie> >(v);
    //Eventually update cookies, may set S
    bool cookieUpdated = false;
    Q_FOREACH (QNetworkCookie cookie, c) {
        qDebug() << cookie.name();
        for (int i=0; i<mSessionCookies.size(); i++) {
            if (mSessionCookies[i].name() == cookie.name()) {
                mSessionCookies[i].setValue(cookie.value());
                Q_EMIT cookieUpdateNeeded(cookie);
                qDebug() << "Updated cookie " << cookie.name();
                cookieUpdated = true;
            }
        }
    }
    if (cookieUpdated) {
        mNetworkAccessManager = new QNetworkAccessManager();
        Q_EMIT qnamUpdated(mNetworkAccessManager);
    }

    QString sreply = reply->read(MAX_READ_BYTES);
    ////qDebug() << "Got reply for lp " << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    ///
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==401) {
        qDebug() << "Auth expired!";
    }
    else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==400) {
        //Need new SID?
        qDebug() << sreply;
        qDebug() << "New seed needed?";
        fetchNewSid();
        qDebug() << "fetched";
        return;
    }

    if (!mLastIncompleteParcel.isEmpty())
        sreply = QString(mLastIncompleteParcel + sreply);
    if (!sreply.endsWith("]\n"))
    {
        qDebug() << "Incomplete parcel";
        mLastIncompleteParcel = sreply;
        return;
    } else {
        mLastIncompleteParcel = "";
    }

    //if I'm here it means the channel is working fine
    if (mChannelError) {
        Q_EMIT channelRestored(mLastPushReceived.addMSecs(500));
        mChannelError = false;
    }
    mLastPushReceived = QDateTime::currentDateTime();

    parseChannelData(sreply);
}

void Channel::parseSid()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QVariant v = reply->header(QNetworkRequest::SetCookieHeader);
    QList<QNetworkCookie> c = qvariant_cast<QList<QNetworkCookie> >(v);
    //Eventually update cookies, may set S
    bool cookieUpdated = false;
    Q_FOREACH (QNetworkCookie cookie, c) {
        qDebug() << cookie.name();
        for (int i=0; i<mSessionCookies.size(); i++) {
            if (mSessionCookies[i].name() == cookie.name()) {
                mSessionCookies[i].setValue(cookie.value());
                Q_EMIT cookieUpdateNeeded(cookie);
                qDebug() << "Updated cookie " << cookie.name();
                cookieUpdated = true;
            }
        }
    }
    if (cookieUpdated) {
        mNetworkAccessManager = new QNetworkAccessManager();
        Q_EMIT qnamUpdated(mNetworkAccessManager);
    }
    if (reply->error() == QNetworkReply::NoError) {
        QString rep = reply->readAll();
        qDebug() << rep;
        int start, start2=1, tmp=1;
        //ROW 0
        start = rep.indexOf("[");
        QString zero = Utils::getNextAtomicField(Utils::getNextAtomicField(rep, start), start2);
        qDebug() << zero;
        //Skip 1
        Utils::getNextAtomicField(zero, tmp);
        zero = Utils::getNextAtomicField(zero, tmp);
        tmp = 1;
        //Skip 1
        Utils::getNextAtomicField(zero, tmp);
        mSid = Utils::getNextAtomicField(zero, tmp);
        mSid = mSid.mid(1, mSid.size()-2);
        qDebug() << mSid;

        //ROW 1 and 2 discarded
        tmp = 1;
        start2 = rep.indexOf("[", start2);
        QString one = Utils::getNextAtomicField(rep, start2);
        qDebug() << one;
        start2 = rep.indexOf("[", start2);
        QString two = Utils::getNextAtomicField(rep, start2);
        qDebug() << two;


        //ROW 3
        start2 = rep.indexOf("[", start2);
        QString three = Utils::getNextAtomicField(rep, start2);
        qDebug() << three;
        //Skip 1
        Utils::getNextAtomicField(three, tmp);
        three = Utils::getNextAtomicField(three, tmp);
        tmp = 1;
        //Skip 1
        Utils::getNextAtomicField(three, tmp);
        three = Utils::getNextAtomicField(three, tmp);
        tmp = 1;
        //Skip 1
        Utils::getNextAtomicField(three, tmp);
        three = Utils::getNextAtomicField(three, tmp);
        tmp = 1;

        Utils::getNextAtomicField(three, tmp);
        mEmail = Utils::getNextAtomicField(three, tmp);
        mEmail = mEmail.mid(1, mEmail.size()-2);
        QStringList temp = mEmail.split("/");
        qDebug() << temp.at(0);
        mEmail = temp.at(0);
        qDebug() << temp.at(1);
        mHeaderClient = temp.at(1);
        Q_EMIT updateClientId(mHeaderClient);

        //ROW 4
        start2 = rep.indexOf("[", start2);
        QString four = Utils::getNextAtomicField(rep, start2);
        qDebug() << four;
        //Skip 1
        Utils::getNextAtomicField(four, tmp);
        four = Utils::getNextAtomicField(four, tmp);
        tmp = 1;
        //Skip 1
        Utils::getNextAtomicField(four, tmp);
        four = Utils::getNextAtomicField(four, tmp);
        tmp = 1;
        //Skip 1
        Utils::getNextAtomicField(four, tmp);
        four = Utils::getNextAtomicField(four, tmp);
        tmp = 1;

        Utils::getNextAtomicField(four, tmp);
        mGSessionId = Utils::getNextAtomicField(four, tmp);
        mGSessionId = mGSessionId.mid(1, mGSessionId.size()-2);

    }
    else {
        QString rep = reply->readAll();
        qDebug() << rep;
        qDebug() << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }
    //Now the reply should be std
    if (!mChannelError) longPollRequest();
}

void Channel::slotError(QNetworkReply::NetworkError err)
{
    mChannelError = true;
    qDebug() << err;
    qDebug() << "Error, retrying to activate channel";
    //longPollRequest();

    /*
    //I may have to retry establishing a connection here, if timers (and only them) are suspended on deep-sleep
    //5 means canceled by user
    if (err != QNetworkReply::OperationCanceledError && err!=QNetworkReply::NetworkSessionFailedError)
        longPollRequest();

    //This may happen temporarily during reconnection
    if (err == QNetworkReply::NetworkSessionFailedError)
        QTimer::singleShot(5000, this, SLOT(LPRSlot()));
        */
}

void Channel::longPollRequest()
{
    if (mLongPoolRequest != NULL) {
        mLongPoolRequest->close();
        delete mLongPoolRequest;
        mLongPoolRequest = NULL;
    }
    //for (;;) {
    QString body = "?VER=8&RID=rpc&t=1&CI=0&clid=" + mClid + "&prop=" + mProp + "&gsessionid=" + mGSessionId + "&SID=" + mSid + "&ec="+mEc;
    QNetworkRequest req(QUrl(QString("https://talkgadget.google.com" + mPath + "bind" + body)));
    req.setRawHeader("User-Agent", QVariant::fromValue(user_agent).toByteArray());
    req.setRawHeader("Connection", "Keep-Alive");

    req.setHeader(QNetworkRequest::CookieHeader, QVariant::fromValue(mSessionCookies));
    qDebug() << "Making lp req";
    mLongPoolRequest = mNetworkAccessManager->get(req);
    QObject::connect(mLongPoolRequest, SIGNAL(readyRead()), this, SLOT(networReadyRead()));
    QObject::connect(mLongPoolRequest, SIGNAL(finished()), this, SLOT(networkRequestFinished()));
    QObject::connect(mLongPoolRequest,SIGNAL(error(QNetworkReply::NetworkError)),this,SLOT(slotError(QNetworkReply::NetworkError)));
    // }
}

void Channel::fetchNewSid()
{
    qDebug() << "fetch new sid";
    QNetworkRequest req(QString("https://talkgadget.google.com" + mPath + "bind"));
    ////qDebug() << req.url().toString();
    //QVariant body = "{\"VER\":8, \"RID\": 81187, \"clid\": \"" + mClid + "\", \"ec\": \"" + mEc + "\", \"prop\": \"" + mProp + "\"}";
    QVariant body = "VER=8&RID=81187&clid=" + mClid + "&prop=" + mProp + "&ec="+mEc;
    ////qDebug() << body.toString();
    QList<QNetworkCookie> reqCookies;
    Q_FOREACH (QNetworkCookie cookie, mSessionCookies) {
        //if (cookie.name()=="SAPISID" || cookie.name()=="SAPISID" || cookie.name()=="HSID" || cookie.name()=="APISID" || cookie.name()=="SID")
        reqCookies.append(cookie);
    }
    req.setHeader(QNetworkRequest::CookieHeader, QVariant::fromValue(reqCookies));
    QNetworkReply *rep = mNetworkAccessManager->post(req, body.toByteArray());
    QObject::connect(rep, SIGNAL(finished()), this, SLOT(parseSid()));
    ////qDebug() << "posted";
}

void Channel::listen()
{
    static int MAX_RETRIES = 1;
    int retries = MAX_RETRIES;
    bool need_new_sid = true;

    while (retries > 0) {
        if (need_new_sid) {
            fetchNewSid();
            need_new_sid = false;
        }
        retries--;
    }
}

QDateTime Channel::getLastPushTs()
{
    return mLastPushReceived;
}

void Channel::setAppOpened()
{
    appPaused = false;
}

void Channel::setAppPaused()
{
    appPaused = true;
}
