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

#ifndef TYPES_H
#define TYPES_H

#include "hangouts.pb.h"

#define USER_AGENT "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/34.0.1847.132 Safari/537.36"
#define SECONDFACTOR_URL "https://accounts.google.com/SecondFactor"
#define SERVICE_LOGIN_AUTH_URL "https://accounts.google.com/ServiceLoginAuth"
#define SERVICE_LOGIN_URL "https://accounts.google.com/ServiceLogin"
#define SERVICE_SMS_AUTH_URL "https://accounts.google.com/signin/challenge"
#define CHAT_INIT_URL "https://talkgadget.google.com/u/0/talkgadget/_/chat"
#define ENDPOINT_URL "https://clients6.google.com/chat/v1/"
#define ORIGIN_URL "https://talkgadget.google.com"

#define OPERATION_NOOP "noop"
#define OPERATION_C "c"
#define OPERATION_CLIENT_BATCH_UPDATE "cbu"

//Timeout to send for setactiveclient requests:
#define ACTIVE_TIMEOUT_SECS 300
//Minimum timeout between subsequent setactiveclient requests:
#define SETACTIVECLIENT_LIMIT_SECS 30

#define MAX_READ_BYTES 1024 * 1024

enum AuthenticationStatus {
    AUTH_WRONG_CREDENTIALS = 0,
    AUTH_NEED_2FACTOR_PIN,
    AUTH_WRONG_2FACTOR_PIN,
    AUTH_CANT_GET_GALX_TOKEN,
    AUTH_UNKNOWN_ERROR
};

enum AuthenticationPhase {
    AUTH_PHASE_INITIAL = 0,
    AUTH_PHASE_GALX_REQUESTED,
    AUTH_PHASE_CREDENTIALS_SENT,
    AUTH_PHASE_GOT_COOKIES,
    AUTH_PHASE_2FACTOR_PIN_SENT,
};

enum ConnectionStatus {
    CONNECTION_STATUS_DISCONNECTED = 0,
    CONNECTION_STATUS_CONNECTING,
    CONNECTION_STATUS_CONNECTED
};

struct OutgoingImage {
    QString filename;
    QString conversationId;
};

#endif // TYPES_H
