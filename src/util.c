#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <uuid/uuid.h>
#include <jansson.h>
#include <zmq.h>

#include "profile.h"

#define UUID_STRING_SIZE 36

char* uuid4() {
    uuid_t uuid;
    char* string = malloc(UUID_STRING_SIZE+1);
    uuid_generate_random(uuid);
    uuid_unparse(uuid, string);
    return string;
}

static const char* json_type_strings[] = {
    "object",
    "array",
    "string",
    "integer",
    "real",
    "true",
    "false",
    "null",
};

const char* json_strof(const json_t* json) {
    switch json_typeof(json) {
        case JSON_OBJECT:  return json_type_strings[0];
        case JSON_ARRAY:   return json_type_strings[1];
        case JSON_STRING:  return json_type_strings[2];
        case JSON_INTEGER: return json_type_strings[3];
        case JSON_REAL:    return json_type_strings[4];
        case JSON_TRUE:    return json_type_strings[5];
        case JSON_FALSE:   return json_type_strings[6];
        case JSON_NULL:    return json_type_strings[7];
    }
}

char* srecv(void* socket) {
    zmq_msg_t message;
    char* string;
    int size;

    zmq_msg_init(&message);
    if (zmq_msg_recv(&message, socket, 0))
        return NULL;

    size = zmq_msg_size(&message);
    string = malloc(size+1);
    memcpy(string, zmq_msg_data(&message), size);
    string[size] = 0;

    zmq_msg_close(&message);
    return string;
}

int ssend(void* socket, const char* string, bool more) {
    zmq_msg_t message;
    int size = strlen(string);
    zmq_msg_init_size(&message, size);
    memcpy(zmq_msg_data(&message), string, size);
    int rc = zmq_msg_send(socket, &message, more ? ZMQ_SNDMORE : 0);
    zmq_msg_close(&message);
    return rc;
}

char* hmac(Profile* profile, const char* s1, ...) {
    const EVP_MD* md;
    HMAC_CTX ctx;
    va_list args;
    char* digest;
    char* s;

    md = EVP_get_digestbyname(profile->signature_scheme);
    if (md == NULL) {
        fprintf(stderr, "error: \"%s\" digest algorithm not supported\n", profile->signature_scheme);
        exit(1);
    }

    HMAC_CTX_init(&ctx);
    HMAC_Init(&ctx, profile->key, strlen(profile->key), md);

    va_start(args, s1);
    while ((s = va_arg(args, char*))) {
        HMAC_Update(&ctx, s, strlen(s));
    }
    va_end(args);

    digest = malloc(EVP_MAX_MD_SIZE+1);
    HMAC_Final(&ctx, digest, NULL);
    HMAC_CTX_cleanup(&ctx);

    return digest;
}