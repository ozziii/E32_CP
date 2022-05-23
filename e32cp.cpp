#include <e32cp.h>
#include "ozaes.h"

#define TAG "E32cp"

e32cp::e32cp() {}

e32cp_errno_t e32cp::begin(e32cp_config_t config)
{
    if (config.lora == NULL)
        return E32CP_ERR_NO_LORA;
    if (config.key_length != 32 && config.key_length != 24 && config.key_length != 16)
        return E32CP_ERR_KEY_LENGTH;

    this->_config = config;

    return E32CP_ERR_SUCCESS;
}

e32cp_errno_t e32cp::wake_up_asleep(String payload, uint16_t address, uint8_t channel)
{
    e32cp_errno_t err;
    uint8_t count = E32CP_SEND_TRY_TIME;

    do
    {
        err = this->_send(E32_MODE_WAKE_UP, payload, address, channel);
        count--;
    } while (err != E32CP_ERR_SUCCESS && count > 0);

    return err;
}

String e32cp::asleep_woke_up()
{
    e32_receve_struct_t message;
    message.buffer = (uint8_t *)malloc(E32CP_CMD_SIZE);
    *(message.buffer) = E32CP_MSG_HANDUP;
    *(message.buffer + 1) = E32_SERVER_ADDRESS;
    *(message.buffer + 2) = E32_SERVER_ADDRESS;
    *(message.buffer + 3) = E32_SERVER_CHANNEL;
    message.size = E32CP_CMD_SIZE;

    return this->_response_handup(message);
}

e32cp_errno_t e32cp::send(String payload, uint16_t address, uint8_t channel)
{
    e32cp_errno_t err;
    uint8_t count = E32CP_SEND_TRY_TIME;

    do
    {
        err = this->_send(E32_MODE_NORMAL, payload, address, channel);
        count--;
    } while (err != E32CP_ERR_SUCCESS && count > 0);

    return err;
}

String e32cp::receve_from(e32_receve_struct_t header)
{
    if (header.size == 0)
        return String();

    switch (*header.buffer)
    {
    case E32CP_MSG_HANDUP:
        return this->_response_handup(header);
        break;
    default:
        break;
    }

    ESP_LOGW(TAG, "Unknow command [%u]", header.buffer);
    return String();
}

String e32cp::_response_handup(e32_receve_struct_t header)
{

    if (header.size != E32CP_CMD_SIZE)
    {
        if (header.buffer != NULL)
            free(header.buffer);

        ESP_LOGW(TAG, "Wrong command size [%u]", header.size);
        return String();
    }

    if (this->_config.lora->set_mode(E32_MODE_NORMAL) != E32_ERR_SUCCESS)
    {
        if (header.buffer != NULL)
            free(header.buffer);
        ESP_LOGW(TAG, "Set Mode error");
        return String();
    }

    uint8_t addH = *(header.buffer + 1);
    uint8_t addL = *(header.buffer + 2);
    uint8_t address = ((uint16_t)addH << 8) | addL;
    uint8_t channel = *(header.buffer + 3);
    free(header.buffer);

    uint8_t *one_time_key = this->_one_time_password();

    unsigned int out_len;
    uint8_t *cry_one_time_key = encrypt_CBC(
        one_time_key,
        this->_config.key_length,
        this->_config.pre_shared_key,
        this->_config.key_length,
        out_len);

    if (cry_one_time_key == nullptr)
    {
        free(one_time_key);
        ESP_LOGW(TAG, "Encrypt one time password error");
        return String();
    }

    e32_errno_t err = this->_config.lora->send(
        address,
        channel,
        cry_one_time_key,
        out_len);

    free(cry_one_time_key);

    if (err != E32_ERR_SUCCESS)
    {
        free(one_time_key);
        ESP_LOGW(TAG, "Send key error [%d]", err);
        return String();
    }

    e32_receve_struct_t cry_message = this->_config.lora->receve(pdMS_TO_TICKS(E32_MESSAGE_DELAY));

    if (cry_message.size == 0)
    {
        free(one_time_key);
        ESP_LOGW(TAG, "Receve message Timeout");
        return String();
    }

    uint8_t *message = decrypt_CBC(cry_message.buffer, cry_message.size, one_time_key, this->_config.key_length);

    free(one_time_key);
    free(cry_message.buffer);

    if (message == nullptr)
    {
        ESP_LOGW(TAG, "Decrypt message error");
        return String();
    }

    String ret;

    for (size_t i = 0; i < cry_message.size; i++)
    {
        char c = *(message + i);
        if (c == 0)
            break;
        ret.concat(c);
    }

    free(message);

    return ret;
}

uint8_t *e32cp::_one_time_password()
{
    uint8_t *newKey = (uint8_t *)malloc(this->_config.key_length);

    if (this->_config.bootloader_random)
        bootloader_random_enable();

    bootloader_fill_random((void *)newKey, this->_config.key_length);

    if (this->_config.bootloader_random)
        bootloader_random_disable();

    return newKey;
}

uint8_t *e32cp::_build_command(e32cp_message_t message)
{
    uint8_t *handu = (uint8_t *)malloc(E32CP_CMD_SIZE);

    uint8_t addL = (uint8_t)(this->_config.address & 0xff);
    uint8_t addH = (uint8_t)((this->_config.address >> 8) & 0xff);

    *(handu) = message;
    *(handu + 1) = addH;
    *(handu + 2) = addL;
    *(handu + 3) = this->_config.channel;

    return handu;
}

e32cp_errno_t e32cp::_send(e32_mode_t mode, String payload, uint16_t address, uint8_t channel)
{
    if (mode != E32_MODE_WAKE_UP && mode != E32_MODE_NORMAL)
    {
        ESP_LOGW(TAG, "Mode not allow");
        return E32CP_ERR_UNKNOWN;
    }

    if (this->_config.lora->set_mode(mode) != E32_ERR_SUCCESS)
    {
        ESP_LOGW(TAG, "Set Mode error");
        return E32CP_ERR_MODE_CHANGE;
    }

    e32cp_message_t message = E32CP_MSG_HANDUP;

    if (mode == E32_MODE_WAKE_UP)
        message = E32CP_MSG_WAKE;

    uint8_t *command = this->_build_command(message);
    e32_errno_t err = this->_config.lora->send(address, channel, command, E32CP_CMD_SIZE);
    free(command);

    vTaskDelay(pdMS_TO_TICKS(100));

    if (this->_config.lora->set_mode(E32_MODE_NORMAL) != E32_ERR_SUCCESS)
    {
        ESP_LOGW(TAG, "Set Mode error");
        return E32CP_ERR_MODE_CHANGE;
    }

    if (err != E32_ERR_SUCCESS)
    {
        ESP_LOGW(TAG, "Send wake error [%d]", err);
        return E32CP_ERR_COMUNICATION;
    }

    unsigned long timetoWait = 0;

    if (mode == E32_MODE_WAKE_UP)
        timetoWait = E32_WAKE_DELAY;
    else
        timetoWait = E32_MESSAGE_DELAY;

    auto cript_key = this->_config.lora->receve(pdMS_TO_TICKS(timetoWait));

    if (cript_key.size == 0)
    {
        ESP_LOGW(TAG, "Receve key Timeout");
        return E32CP_ERR_COMUNICATION;
    }

    if (cript_key.size != this->_config.key_length)
    {
        ESP_LOGW(TAG, "Wrong key length");
        free(cript_key.buffer);
        return E32CP_ERR_KEY_LENGTH;
    }

    uint8_t *one_time_key = decrypt_CBC(cript_key.buffer, cript_key.size, this->_config.pre_shared_key, this->_config.key_length);
    free(cript_key.buffer);
    if (one_time_key == nullptr)
    {
        ESP_LOGW(TAG, "Decrypt key error");
        return E32CP_ERR_DECRYPT;
    }

    unsigned int out_len;
    uint8_t *cript_massage = encrypt_CBC((uint8_t *)payload.c_str(), payload.length(), one_time_key, this->_config.key_length, out_len);
    free(one_time_key);
    if (cript_massage == nullptr)
    {
        ESP_LOGW(TAG, "Encrypt message error");
        return E32CP_ERR_ENCYPT;
    }

    err = this->_config.lora->send(address, channel, cript_massage, out_len);
    free(cript_massage);

    if (err != E32_ERR_SUCCESS)
    {
        ESP_LOGW(TAG, "Send message error [%d]", err);
        return E32CP_ERR_COMUNICATION;
    }
    else
        return E32CP_ERR_SUCCESS;
}