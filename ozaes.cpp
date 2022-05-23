#include "ozaes.h"

#include "stdlib.h"

#include "aes/esp_aes.h"
#include <string.h>

uint8_t *normalize(uint8_t in[], unsigned int inLen, unsigned int alignLen)
{
    uint8_t *alignIn = (uint8_t *)malloc(sizeof(uint8_t) * alignLen);
    if (alignIn==NULL)
        return alignIn;

    memcpy(alignIn, in, inLen);
    memset(alignIn + inLen, 0x00, alignLen - inLen);
    return alignIn;
}

unsigned int get_normalized_length(unsigned int len)
{
    const uint8_t blockBytesLen = 16;
    unsigned int lengthWithPadding = (len / blockBytesLen);
    if (len % blockBytesLen)
    {
        lengthWithPadding++;
    }

    lengthWithPadding *= blockBytesLen;

    return lengthWithPadding;
}

uint8_t * encrypt_CBC(uint8_t *input, unsigned int input_length, uint8_t *key, unsigned int key_length, unsigned int &out_length)
{
    if (input_length == 0)
        return nullptr;
    if (key_length != 32 && key_length != 24 && key_length != 16)
        return nullptr;

    esp_aes_context ctx;
    esp_aes_init(&ctx);
    int err = esp_aes_setkey(&ctx, key, key_length * 8);

    if (err == ERR_ESP_AES_INVALID_KEY_LENGTH)
    {
        return nullptr;
    }

    uint8_t *iv = (uint8_t *)malloc(sizeof(uint8_t) * 16);
    if (iv==NULL)
        return nullptr;
    memcpy(iv, key, 16);

    out_length = get_normalized_length(input_length);

    uint8_t *payload;

    if(out_length == input_length)
        payload = input;
    else
    {
        payload = normalize(input, input_length, out_length);
        if(normalize == NULL)
            return nullptr;
    }

    uint8_t *encrypted = new uint8_t[out_length];
    err = esp_aes_crypt_cbc(&ctx, ESP_AES_ENCRYPT, out_length, iv, payload, (uint8_t *)encrypted);

    if(out_length != input_length)
        free(payload);

    free(iv);

    if (err == ERR_ESP_AES_INVALID_INPUT_LENGTH)
    {
        return nullptr;
    }

    return encrypted;
}

uint8_t *decrypt_CBC(uint8_t *input, unsigned int input_length, uint8_t *key, unsigned int key_length)
{
    if (input_length == 0 || input_length % 16 != 0)
        return nullptr;
    if (key_length != 32 && key_length != 24 && key_length != 16)
        return nullptr;

    esp_aes_context ctx;
    esp_aes_init(&ctx);
    int err = esp_aes_setkey(&ctx, key, key_length * 8);

    if (err == ERR_ESP_AES_INVALID_KEY_LENGTH)
    {
        return nullptr;
    }

    uint8_t *iv = (uint8_t *)malloc(sizeof(uint8_t) * 16);
    if (iv==NULL)
        return nullptr;
    memcpy(iv, key, 16);

    uint8_t *decrypted = new uint8_t[input_length];
    err = esp_aes_crypt_cbc(&ctx, ESP_AES_DECRYPT, input_length, iv, (uint8_t *)input, (uint8_t *)decrypted);

    free(iv);

    if (err == ERR_ESP_AES_INVALID_INPUT_LENGTH)
    {
        return nullptr;
    }

    return decrypted;
}
