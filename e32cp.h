#ifndef E32CP_h
#define E32CP_h

extern "C"
{
#include <bootloader_random.h>
}

#include "e32oz.h"
#include "ozaes.h"

#define E32_SERVER_ADDRESS 0
#define E32_SERVER_CHANNEL 3
#define E32_WAKE_DELAY 10000
#define E32_MESSAGE_DELAY 1000
#define E32_BROADCAST_ADDRESS 65535

#define E32CP_CMD_SIZE 0x04

#define E32CP_SEND_TRY_TIME 0x03

typedef enum
{
    E32CP_MSG_HANDUP = 0x1A,
    E32CP_MSG_WAKE = 0x1B,

} e32cp_message_t;

typedef enum
{
    E32CP_ERR_SUCCESS = 0,
    E32CP_ERR_UNKNOWN = -1,
    E32CP_ERR_INVALID_PIN = -2,
    E32CP_ERR_INVALID_ADDRESS = -3,
    E32CP_ERR_INVALID_CHANNEL = -4,
    E32CP_ERR_INVALID_UARTNUM = -4,
    E32CP_ERR_TIMEOUT = -5,
    E32CP_ERR_MODE_CHANGE = -6,
    E32CP_ERR_COMUNICATION = -7,
    E32CP_ERR_WRONG_MODE = -8,
    E32CP_ERR_MESSAGE_SIZE = -9,
    E32CP_ERR_CRC = -10,
    E32CP_ERR_QUEUE_CREATE = -11,
    E32CP_ERR_NO_LORA = -12,
    E32CP_ERR_KEY_LENGTH = -13,
    E32CP_ERR_DECRYPT = -14,
    E32CP_ERR_ENCYPT = -15,

} e32cp_errno_t;

typedef struct
{
    e32oz *lora;                    /*!< */
    uint8_t *pre_shared_key;        /*!< */
    uint8_t key_length;             /*!< */
    bool bootloader_random = false; /*!< */
    uint16_t address;
    uint8_t channel;
} e32cp_config_t;

class e32cp
{
public:
    /**
     * @brief Construct a new e32cp object
     *
     */
    e32cp();

    /**
     * @brief initialize uart and set e32 in normal mode call on normal reboot and after esp32 is wake up
     *
     * @param config
     * @return true if sucess
     * @return false on error
     */
    e32cp_errno_t begin(e32cp_config_t config);


    /**
     * @brief  Call to send command to a asleep client
     * Send wake command to asleep client and wait for key. on Key recive send cripted payload
     * Use combined with @asleep_woke_up()
     * Normaly used by server
     *
     * Send Wake --> wait for Key --> Send cripted Payload
     *
     * @param address client address
     * @param payload message to send to the client
     * @return false on error
     *
     */
    e32cp_errno_t wake_up_asleep(String payload, uint16_t address, uint8_t channel);

    /**
     * @brief After esp32 wake up send key to server and wait for payload
     * use compined with @wake_up_asleep(uint16_t address, uint8_t channel, String payload)
     * Normaly used by client
     *
     * Wake by interrupt -->  Send key --> wait for payload
     *
     * @return String payload form server
     */
    String asleep_woke_up();

    /**
     * @brief Send mesage to reciver
     * Normaly used by client
     *
     * Send Request --> wait for key --> send cripted payload
     *
     *
     * @param payload message to send to the client
     * @return false on error
     */
    e32cp_errno_t send(String payload, uint16_t address = E32_SERVER_ADDRESS, uint8_t channel = E32_SERVER_CHANNEL);

    /**
     * @brief 
     * 
     * @param header 
     * @return String 
     */
    String receve_from(e32_receve_struct_t header);

private:


    e32cp_config_t _config;

    uint8_t * _build_command(e32cp_message_t message);

    String _response_handup(e32_receve_struct_t header);

    e32cp_errno_t _send(e32_mode_t mode, String payload, uint16_t address, uint8_t channel);

    uint8_t *_one_time_password();
};

#endif