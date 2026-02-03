#include "jwt.h"
#include "jwt_config.h"
#include "utils.h"

#include "esp_http_client.h"
#include "esp_crt_bundle.h"

#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include "mbedtls/base64.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"

#include "cJSON.h"

#include "esp_log.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>

char *g_access_token = NULL;
time_t g_token_expiry = 0;

char* create_jwt(void)
{
    time_t now = time(NULL);
    
    // Build header
    cJSON *h = cJSON_CreateObject();
    cJSON_AddStringToObject(h, "alg", "RS256");
    cJSON_AddStringToObject(h, "typ", "JWT");
    char *h_str = cJSON_PrintUnformatted(h);
    cJSON_Delete(h);

    // Build payload
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "iss", JWT_SERVICE_ACCOUNT_EMAIL);
    cJSON_AddStringToObject(p, "scope", JWT_SCOPE);
    cJSON_AddStringToObject(p, "aud", JWT_TOKEN_URI);
    cJSON_AddNumberToObject(p, "iat", (double)now);
    cJSON_AddNumberToObject(p, "exp", (double)(now + 3600));
    char *p_str = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    // Encode header and payload
    char *h_b64 = base64url_encode((unsigned char*)h_str, strlen(h_str));
    char *p_b64 = base64url_encode((unsigned char*)p_str, strlen(p_str));
    free(h_str);
    free(p_str);

    // Create unsigned JWT
    size_t unsigned_len = strlen(h_b64) + strlen(p_b64) + 2;
    char *unsigned_jwt = malloc(unsigned_len);
    snprintf(unsigned_jwt, unsigned_len, "%s.%s", h_b64, p_b64);
    free(h_b64);
    free(p_b64);

    // Sign JWT
    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr;
    unsigned char hash[32];
    unsigned char sig[512];
    size_t sig_len = 0;

    mbedtls_pk_init(&pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr);

    mbedtls_pk_parse_key(&pk, (const unsigned char*)JWT_SERVICE_ACCOUNT_PRIVATE_KEY_PEM, 
                         strlen(JWT_SERVICE_ACCOUNT_PRIVATE_KEY_PEM) + 1, NULL, 0, NULL, NULL);
    mbedtls_ctr_drbg_seed(&ctr, mbedtls_entropy_func, &entropy, 
                          (const unsigned char*)"esp32", 5);

    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md(md, (const unsigned char*)unsigned_jwt, strlen(unsigned_jwt), hash);
    mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), sig, 
                    sizeof(sig), &sig_len, mbedtls_ctr_drbg_random, &ctr);

    char *sig_b64 = base64url_encode(sig, sig_len);

    // Complete JWT
    size_t jwt_len = strlen(unsigned_jwt) + strlen(sig_b64) + 2;
    char *jwt = malloc(jwt_len);
    snprintf(jwt, jwt_len, "%s.%s", unsigned_jwt, sig_b64);

    free(unsigned_jwt);
    free(sig_b64);
    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr);
    mbedtls_entropy_free(&entropy);

    return jwt;
}

esp_http_client_handle_t http_jwt_init(void)
{
    esp_http_client_config_t cfg = {
        .url = JWT_TOKEN_URI,
        .method = HTTP_METHOD_POST,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .keep_alive_enable = true,
    };

    return esp_http_client_init(&cfg);
}

void http_jwt_deinit(esp_http_client_handle_t client)
{
    esp_http_client_cleanup(client);
}

char* fetch_access_token(esp_http_client_handle_t client)
{
    if (!client) 
    {
        ESP_LOGE("JWT","JWT HTTP Init failed");
        return NULL;
    }
    else ESP_LOGI("JWT","JWT HTTP Init success");

    char *jwt = create_jwt();
    if (!jwt) return NULL;

    char *enc_jwt = url_encode(jwt);
    free(jwt);
    if (!enc_jwt) return NULL;

    const char *prefix = "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=";
    size_t body_len = strlen(prefix) + strlen(enc_jwt) + 1;
    char *body = malloc(body_len);
    snprintf(body, body_len, "%s%s", prefix, enc_jwt);
    free(enc_jwt);

    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_http_client_open(client, strlen(body));
    esp_http_client_write(client, body, strlen(body));
    esp_http_client_fetch_headers(client);

    char buf[HTTP_BUFFER_SIZE];
    int len = esp_http_client_read(client, buf, sizeof(buf) - 1);
    buf[len > 0 ? len : 0] = '\0';

    esp_http_client_close(client);
    free(body);

    cJSON *root = cJSON_Parse(buf);
    if (!root) return NULL;

    cJSON *token = cJSON_GetObjectItem(root, "access_token");
    cJSON *expires = cJSON_GetObjectItem(root, "expires_in");
    
    char *result = token ? strdup(token->valuestring) : NULL;
    int exp_time = expires ? expires->valueint : 3600;
    
    if (result) {
        g_token_expiry = time(NULL) + exp_time - TOKEN_REFRESH_MARGIN_SEC;
        if (g_access_token) free(g_access_token);
        g_access_token = strdup(result);
    }

    cJSON_Delete(root);
    return result;
}

const char* get_access_token(esp_http_client_handle_t jwt_http_handle)
{
    time_t now = time(NULL);
    if (g_access_token && now < g_token_expiry) {
        return g_access_token;
    }
    
    char *token = fetch_access_token(jwt_http_handle);
    if (token) free(token);
    return g_access_token;
}