#pragma once


#define JWT_SCOPE \
    "https://www.googleapis.com/auth/calendar.readonly"

#define JWT_TOKEN_URI \
    "https://oauth2.googleapis.com/token"

extern const char *JWT_CALENDAR_ID;

extern const char *JWT_SERVICE_ACCOUNT_EMAIL;

extern const char *JWT_SERVICE_ACCOUNT_PRIVATE_KEY_PEM;
