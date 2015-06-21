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

#ifndef AUTHENTICATOR_H
#define AUTHENTICATOR_H

#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QUrl>
#include "types.h"

class Authenticator : public QObject
{
    Q_OBJECT

public:
    Authenticator(const QString &cookiePath);
    void updateCookieFile(const QList<QNetworkCookie> &cookies);
    void authenticate();
    void sendCredentials(const QString &uname, const QString &passwd);
    void send2ndFactorPin(const QString &pin);
    void getGalxToken();

public Q_SLOTS:
    void networkCallback(QNetworkReply *reply);

Q_SIGNALS:
    void loginNeeded();
    void gotCookies(QMap<QString, QNetworkCookie> cookies);
    void authFailed(AuthenticationStatus status, QString error = QString::null);

private:
    void saveAuthCookies();
    void followRedirection(QUrl url);
    bool amILoggedIn() const;

    QMap<QString, QNetworkCookie> mSessionCookies;
    QNetworkAccessManager mNetworkAccessManager;
    QNetworkCookie mGALXCookie;
    int mAuthPhase;
    QString mCookiePath;
    QString mSecTok;
    QString mTimeStmp;
};

#endif // AUTHENTICATOR_H
