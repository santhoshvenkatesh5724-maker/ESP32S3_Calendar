#include <stdlib.h>
#include <string.h>
#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include "mbedtls/base64.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"

#include "utils.h"

char* base64url_encode(const unsigned char *input, size_t ilen)
{
    size_t olen = 0;
    mbedtls_base64_encode(NULL, 0, &olen, input, ilen);
    unsigned char *b64 = malloc(olen + 1);
    if (!b64) return NULL;
    
    mbedtls_base64_encode(b64, olen, &olen, input, ilen);
    b64[olen] = 0;
    
    for (size_t i = 0; i < olen; ++i) {
        if (b64[i] == '+') b64[i] = '-';
        else if (b64[i] == '/') b64[i] = '_';
    }
    
    while (olen > 0 && b64[olen-1] == '=') olen--;
    b64[olen] = 0;
    
    char *ret = strdup((char*)b64);
    free(b64);
    return ret;
}

char* url_encode(const char *src)
{
    if (!src) return NULL;
    size_t len = strlen(src);
    char *out = malloc(len * 3 + 1);
    if (!out) return NULL;
    
    char *p = out;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = src[i];
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || 
            (c >= 'a' && c <= 'z') || c == '-' || c == '.' || c == '_' || c == '~') {
            *p++ = c;
        } else {
            sprintf(p, "%%%02X", c);
            p += 3;
        }
    }
    *p = '\0';
    return out;
}