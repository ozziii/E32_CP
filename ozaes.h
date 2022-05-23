#ifndef OZ_AES_H
#define OZ_AES_H

#include "stdint.h"

uint8_t * encrypt_CBC(uint8_t *input, unsigned int input_length, uint8_t *key, unsigned int key_length, unsigned int &out_length);

uint8_t * decrypt_CBC(uint8_t *input, unsigned int input_length, uint8_t *key, unsigned int key_length);

#endif