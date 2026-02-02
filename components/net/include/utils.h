#pragma once

#include <stddef.h>

char* base64url_encode(const unsigned char *input, size_t ilen);
char* url_encode(const char *src);

