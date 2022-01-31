/*
 * E32 COMUNICATION PROTOCOL
 *
 * AUTHOR:  OZIIII
 * VERSION: 1.0.0
 *
 * The MIT License (MIT)
 *
 * You may copy, alter and reuse this code in any way you like, but please leave
 * reference to www.mischianti.org in your comments if you redistribute this code.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef E32CP_h
#define E32CP_h

#include "Arduino.h"
#include <functional>

extern "C"
{
#include <bootloader_random.h>
}

#include "driver/uart.h"

#include <LoRa_E32.h>

#include <ozaes.h>

// Setup debug printing macros.
//#define E32CP_DEBUG

#ifdef E32CP_DEBUG
#define E32CP_LOGD(format, ...)  ESP_LOGD("E32CP", format, ##__VA_ARGS__)
#define E32CP_LOGI(format, ...)  ESP_LOGI("E32CP", format, ##__VA_ARGS__) 
#define E32CP_LOGW(format, ...)  ESP_LOGW("E32CP", format, ##__VA_ARGS__) 
#define E32CP_LOGE(format, ...)  ESP_LOGE("E32CP", format, ##__VA_ARGS__) 
#else
#define E32CP_LOGD(format, ...) \
    {                           \
    }
#define E32CP_LOGI(format, ...) \
    {                           \
    }
#define E32CP_LOGW(format, ...) \
    {                           \
    }
#define E32CP_LOGE(format, ...) \
    {                           \
    }
#endif

// SERVER ADDRESS
#define E32_SERVER_ADDRESS 0
#define E32_SERVER_CHANNEL 3
#define E32_WAKE_COMMAND "wake"
#define E32_HANDUP_COMMAND "hup"
#define E32_HANDUP_SEPARATOR_CHAR 47
#define E32_WAKE_DELAY 4000

typedef struct
{
    LoRa_E32 *lora;                        /*!< */
    uint8_t *pre_shared_key;               /*!< */
    uint8_t key_length;                    /*!< */
    uint16_t address = E32_SERVER_ADDRESS; /*!< */
    uint8_t channel = E32_SERVER_CHANNEL;  /*!< */
    bool bootloader_random = false;        /*!< */
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
    bool begin(e32cp_config_t config);

    /**
     * @brief Configure e32 not call this functiono afret esp32 wake up ONLY after rebooy
     *
     *
     * @return true  if sucess
     * @return false on error
     */
    bool configure();

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
    bool wake_up_asleep(String payload, uint16_t address);

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
    bool send(String payload, uint16_t address = E32_SERVER_ADDRESS);

    /**
     * @brief recive data from client call this function in loop()
     * Normaly used by server
     *
     * On recieve request -> send key -> recieve data
     *
     * @return String if data is avalable else empty if data not coming
     */
    String recive();

private:
    String _server_recieve();

    uint8_t *_one_time_password();
    uint16_t _address;
    uint8_t _channel, _key_length;
    LoRa_E32 *_lora;
    bool _bootloader_random;
    uint8_t *_pre_shared_key;
    SemaphoreHandle_t uar_mutex;
};

#endif