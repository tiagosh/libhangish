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

#include "hangouts.pb.h"

#include <QCoreApplication>
#include <QDebug>
#include <QScriptEngine>
#include <QScriptValue>
#include <QVariantList>
#include <QFile>
#include <QUrlQuery>

#include "channel.h"

Channel::Channel(const QMap<QString, QNetworkCookie> &cookies, const QString &ppath, const QString &pclid, const QString &pec, const QString &pprop, ClientEntity pms) :
    mLongPoolRequest(NULL),
    mChannelError(false),
    mMyself(pms),
    mSessionCookies(cookies),
    mClid(pclid),
    mEc(pec),
    mPath(ppath),
    mProp(pprop),
    mLastPushReceived(0),
    mPendingParcelSize(0),
    mCheckChannelTimer(new QTimer(this)),
    mFetchingSid(false)
{
    QObject::connect(mCheckChannelTimer, SIGNAL(timeout()), this, SLOT(onChannelLost()));
}

void Channel::processCookies(QNetworkReply *reply)
{
    bool cookieUpdated = false;
    QVariant v = reply->header(QNetworkRequest::SetCookieHeader);
    QList<QNetworkCookie> c = qvariant_cast<QList<QNetworkCookie> >(v);

    Q_FOREACH (QNetworkCookie cookie, c) {
        if (mSessionCookies.contains(cookie.name())) {
            mSessionCookies[cookie.name()] = cookie;
            Q_EMIT cookieUpdateNeeded(cookie);
            cookieUpdated = true;
        }
    }

    if (cookieUpdated) {
        mNetworkAccessManager.setCookieJar(new QNetworkCookieJar(this));
    }
}

void Channel::onChannelLost()
{
    qDebug() << "Dead, here I should sync al evts from last ts";
    qDebug() << "start new lpconn";
    mCheckChannelTimer->stop();
    mChannelError = true;
    Q_EMIT channelLost();
    QTimer::singleShot(500, this, SLOT(longPollRequest()));
}

void Channel::parseChannelData(const QString &sreply)
{
    QVariantList submission = Utils::jsArrayToVariantList(sreply);
    QString operation = submission[0].toList()[1].toList()[0].toString();
    if (operation == OPERATION_NOOP || operation != OPERATION_C) {
        return;
    }

    // TODO: create a protobuf message to parse this
    if (submission[0].toList()[1].toList()[1].toList()[1].toList()[0].toString() != "bfo") {
        return;
    }

    QString tmp = submission[0].toList()[1].toList()[1].toList()[1].toList()[1].toString();
    QVariantList payload = Utils::jsArrayToVariantList(tmp);

    // for now we only process client batch updates
    if (payload[0].toString() != OPERATION_CLIENT_BATCH_UPDATE) {
        return;
    }

    ClientBatchUpdate cbu;
    Utils::packToMessage(QVariantList() << payload[1], cbu);
    Q_EMIT clientBatchUpdate(cbu);

    if (cbu.stateupdate(cbu.stateupdate_size()-1).has_stateupdateheader()) {
        mLastPushReceived = cbu.stateupdate(cbu.stateupdate_size()-1).stateupdateheader().currentservertime();
    }
    qDebug() << "Message: " << cbu.DebugString().c_str();
}

void Channel::longPollRequest()
{
    qDebug() << __func__;

    if (mLongPoolRequest != NULL) {
        mLongPoolRequest->close();
        mLongPoolRequest->deleteLater();
        mLongPoolRequest = NULL;
    }

    QUrlQuery query;
    query.addQueryItem("VER", "8");
    query.addQueryItem("RID", "rpc");
    query.addQueryItem("t", "1");
    query.addQueryItem("CI", "0");
    query.addQueryItem("ctype", "hangouts");
    query.addQueryItem("TYPE", "xmlhttp");
    query.addQueryItem("clid", mClid);
    query.addQueryItem("prop", mProp);
    query.addQueryItem("gsessionid", mGSessionId);
    query.addQueryItem("SID", mSid);
    query.addQueryItem("ec", mEc);

    QUrl url(QString("https://talkgadget.google.com" + mPath + "bind"));
    url.setQuery(query);
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", QByteArray(USER_AGENT));
    req.setRawHeader("Connection", "Keep-Alive");
    req.setHeader(QNetworkRequest::CookieHeader, QVariant::fromValue(mSessionCookies.values()));

    mLongPoolRequest = mNetworkAccessManager.get(req);
    QObject::connect(mLongPoolRequest, SIGNAL(readyRead()), this, SLOT(networReadyRead()));
    QObject::connect(mLongPoolRequest, SIGNAL(finished()), this, SLOT(networkRequestFinished()));
    QObject::connect(mLongPoolRequest, SIGNAL(error(QNetworkReply::NetworkError)), this ,SLOT(slotError(QNetworkReply::NetworkError)));
}

void Channel::slotError(QNetworkReply::NetworkError err)
{
    qDebug() << __func__ << err;
    mChannelError = true;
    if (err == QNetworkReply::NetworkSessionFailedError) {
        mNetworkAccessManager.setCookieJar(new QNetworkCookieJar(this));
        Q_EMIT cookieUpdateNeeded(QNetworkCookie());
    }
    mCheckChannelTimer->start();
}

void Channel::networReadyRead()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());

    processCookies(reply);

    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==401) {
        qDebug() << "Auth expired!";
    } else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==400) {
        qDebug() << "New seed needed?";
        fetchNewSid();
        return;
    }

    mPendingParcelBuffer += reply->readAll();
    QTextStream stream(mPendingParcelBuffer);
    if (mPendingParcelSize == 0) {
        while (!stream.atEnd()) {
            mPendingParcelSize = stream.readLine(MAX_READ_BYTES).toInt();
            QString buffer = stream.read(mPendingParcelSize);
            if (buffer.size() == mPendingParcelSize) {
                parseChannelData(buffer);
                mPendingParcelSize = 0;
            } else {
                mPendingParcelBuffer = buffer.toUtf8();
                break;
            }
        }
        if (mPendingParcelSize == 0) {
            mPendingParcelBuffer.clear();
        }
    } else {
        QString buffer = stream.read(mPendingParcelSize);
        if (buffer.size() == mPendingParcelSize) {
            parseChannelData(buffer);
            mPendingParcelSize = 0;
            while (!stream.atEnd()) {
                mPendingParcelSize = stream.readLine(MAX_READ_BYTES).toInt();
                buffer = stream.read(mPendingParcelSize);
                if (buffer.size() == mPendingParcelSize) {
                    parseChannelData(buffer);
                    mPendingParcelSize = 0;
                } else {
                    mPendingParcelBuffer = buffer.toUtf8();
                    break;
                }
            }
        } else {
            mPendingParcelBuffer = buffer.toUtf8();
        }

        if (mPendingParcelSize == 0) {
            mPendingParcelBuffer.clear();
        }
    }

    // zero timer on every new message received
    mCheckChannelTimer->start(30000);

    //if I'm here it means the channel is working fine
    if (mChannelError) {
        Q_EMIT channelRestored(mLastPushReceived);
        mChannelError = false;
    }
}

void Channel::networkRequestFinished()
{
    qDebug() << __func__ << mChannelError;

    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());

    processCookies(reply);

    QString srep = reply->readAll();

    if (srep.contains("Unknown SID")) {
        //Need new SID
        fetchNewSid();
        return;
    }

    //If there's a network problem don't do anything, the connection will be retried by checkChannelStatus
    if (!mChannelError) {
        longPollRequest();
    } else {
        mCheckChannelTimer->start(30000);
    }
}

void Channel::fetchNewSid()
{
    qDebug() << __func__ << mFetchingSid;

    if (mFetchingSid) {
        return;
    }

    mFetchingSid = true;

    QNetworkRequest req(QString("https://talkgadget.google.com" + mPath + "bind"));

    QUrlQuery query;
    query.addQueryItem("VER", "8");
    query.addQueryItem("RID", "81187");
    query.addQueryItem("clid", mClid);
    query.addQueryItem("prop", mProp);
    query.addQueryItem("ec", mEc);

    req.setHeader(QNetworkRequest::CookieHeader, QVariant::fromValue(mSessionCookies.values()));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QVariant::fromValue(QString("application/x-www-form-urlencoded")));
    QNetworkReply *rep = mNetworkAccessManager.post(req, query.toString().toLatin1());
    QObject::connect(rep, SIGNAL(finished()), this, SLOT(onFetchNewSidReply()));
}

void Channel::onFetchNewSidReply()
{
    qDebug() << __func__;

    mFetchingSid = false;

    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());

    processCookies(reply);

    if (reply->error() == QNetworkReply::NoError) {
        // drop first line (character count)
        reply->readLine();
        QString rep = reply->readAll();
        qDebug() << "new SID reply " << rep;
        QVariantList sidResponse = Utils::jsArrayToVariantList(rep);
        // first contains the new sid only
        QVariantList sidOp = sidResponse.takeFirst().toList();
        mSid = sidOp[1].toList()[1].toString();

        // iterate over the other lines
        Q_FOREACH(const QVariant &line, sidResponse) {
            QString prop = line.toList()[1].toList()[0].toString();
            if (prop == "b") {
                continue;
            } else if (prop == "c") {
                QVariantList prop2 = line.toList()[1].toList()[1].toList();
                QString propName = prop2[1].toList()[0].toString();
                if (propName == "cfj") {
                    QStringList emailAndHeaderId = prop2[1].toList()[1].toString().split("/");
                    mEmail = emailAndHeaderId.at(0);
                    mHeaderClient = emailAndHeaderId.at(1);
                    Q_EMIT updateClientId(mHeaderClient);
                } else if (propName == "ei") {
                    mGSessionId = prop2[1].toList()[1].toString();
                }
            }

        }
    } else {
        QString rep = reply->readAll();
        qDebug() << rep;
        qDebug() << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }
    //Now the reply should be std
    if (!mChannelError) {
        longPollRequest();
    } else {
        mCheckChannelTimer->start(30000);
    }
}

void Channel::listen()
{
    // start checking if the channel is alive
    mCheckChannelTimer->start(30000);

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

quint64 Channel::getLastPushTs() const
{
    return mLastPushReceived;
}
