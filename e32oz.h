#ifndef E32OZ_h
#define E32OZ_h

#include "Arduino.h"


#define E32_COMMAND_C0 0xC0
#define E32_COMMAND_C1 0xC1
#define E32_COMMAND_C2 0xC2
#define E32_COMMAND_C3 0xC3
#define E32_COMMAND_C4 0xC4

#define E32_MAX_ADDRESS 0xFFFF
#define E32_MAX_CHANNEL 0x1F

#define E32_MAX_UARTE_BUFFER_SIZE 512

#define E32_MAX_MESSAGE_SIZE 58

#define E32_UART_BUFFER_SIZE 512
#define E32_UART_BAUDRATE 9600

#define E32_AUX_SLEEP_TIMEOUT 10

#define E32_BITMASK_UARTPARITY 0xC0
#define E32_BITMASK_UARTBAUD 0x38
#define E32_BITMASK_AIRDATARATE 0x07

#define E32_BITMASK_FIXEDMODE 0x80
#define E32_BITMASK_IODRIVER 0x40
#define E32_BITMASK_WAKEUP 0x38
#define E32_BITMASK_FEC 0x04
#define E32_BITMASK_POWER 0x03
#define E32_BITMASK_CHANNEL 0x1F

/**
 * @brief
 *
 */
typedef enum
{
    E32_ERR_SUCCESS = 0,
    E32_ERR_UNKNOWN = -1,
    E32_ERR_INVALID_PIN = -2,
    E32_ERR_INVALID_ADDRESS = -3,
    E32_ERR_INVALID_CHANNEL = -4,
    E32_ERR_INVALID_UARTNUM = -4,
    E32_ERR_TIMEOUT = -5,
    E32_ERR_MODE_CHANGE = -6,
    E32_ERR_COMUNICATION = -7,
    E32_ERR_WRONG_MODE = -8,
    E32_ERR_MESSAGE_SIZE = -9,
    E32_ERR_CRC = -10,
    E32_ERR_QUEUE_CREATE = -11,
    E32_ERR_UART = -12,
} e32_errno_t;

/**
 * @brief
 *
 */
typedef enum
{
    E32_MODE_NORMAL,
    E32_MODE_WAKE_UP,
    E32_MODE_POWER_SAVING,
    E32_MODE_SLEEP
} e32_mode_t;

/**
 * @brief
 *
 */
typedef enum
{
    E32_PARAMETER_8N1 = 0x00,
    E32_PARAMETER_801 = 0x40,
    E32_PARAMETER_8E1 = 0x80,
} e32_uart_parity_t;

/**
 * @brief
 *
 */
typedef enum
{
    E32_PARAMETER_BAUD_1200 = 0x00,
    E32_PARAMETER_BAUD_2400 = 0x08,
    E32_PARAMETER_BAUD_4800 = 0x10,
    E32_PARAMETER_BAUD_9600 = 0x18,
    E32_PARAMETER_BAUD_19200 = 0x20,
    E32_PARAMETER_BAUD_38400 = 0x28,
    E32_PARAMETER_BAUD_57600 = 0x30,
    E32_PARAMETER_BAUD_115200 = 0x38,
} e32_uart_baud_rate_t;

/**
 * @brief
 *
 */
typedef enum
{
    E32_PARAMETER_AIRATE_03 = 0x00,
    E32_PARAMETER_AIRATE_12 = 0x01,
    E32_PARAMETER_AIRATE_24 = 0x02,
    E32_PARAMETER_AIRATE_48 = 0x03,
    E32_PARAMETER_AIRATE_96 = 0x04,
    E32_PARAMETER_AIRATE_192 = 0x05,
} e32_air_data_rate_t;

/**
 * @brief
 *
 */
typedef enum
{
    E32_OPTION_MODE_TRASPARENT = 0x00,
    E32_OPTION_MODE_FIXED = 0x80,
} e32_fixed_mode_t;

/**
 * @brief
 *
 */
typedef enum
{
    E32_OPTION_IOMODE_PUSHPULL = 0x40,
    E32_OPTION_IOMODE_OPENCOLLECTOR = 0x00,
} e32_io_driver_mode_t;

/**
 * @brief
 *
 */
typedef enum
{
    E32_OPTION_WAKEUP_0250 = 0x00,
    E32_OPTION_WAKEUP_0500 = 0x08,
    E32_OPTION_WAKEUP_0750 = 0x10,
    E32_OPTION_WAKEUP_1000 = 0x18,
    E32_OPTION_WAKEUP_1250 = 0x20,
    E32_OPTION_WAKEUP_1500 = 0x28,
    E32_OPTION_WAKEUP_1750 = 0x30,
    E32_OPTION_WAKEUP_2000 = 0x38,
} e32_wake_up_time_t;

/**
 * @brief
 *
 */
typedef enum
{
    E32_OPTION_FEC_OFF = 0x00,
    E32_OPTION_FEC_ON = 0x04,
} e32_fec_enabled_t;

/**
 * @brief
 *
 */
typedef enum
{
    E32_OPTION_TXPOWER_4 = 0x00,
    E32_OPTION_TXPOWER_3 = 0x01,
    E32_OPTION_TXPOWER_2 = 0x02,
    E32_OPTION_TXPOWER_1 = 0x03,
} e32_transmission_power_t;

typedef struct
{
    e32_errno_t error = E32_ERR_SUCCESS;
    uint8_t channel = 1;
    uint8_t address_H = 0;
    uint8_t address_L = 0;
    e32_uart_parity_t uart_parity = E32_PARAMETER_8N1;
    e32_uart_baud_rate_t uart_baud_rate = E32_PARAMETER_BAUD_9600;
    e32_air_data_rate_t air_data_rate = E32_PARAMETER_AIRATE_24;
    e32_fixed_mode_t fixed_mode = E32_OPTION_MODE_FIXED;
    e32_io_driver_mode_t io_driver_mode = E32_OPTION_IOMODE_PUSHPULL;
    e32_wake_up_time_t wake_up_time = E32_OPTION_WAKEUP_0250;
    e32_fec_enabled_t fec_enabled = E32_OPTION_FEC_ON;
    e32_transmission_power_t transmission_power = E32_OPTION_TXPOWER_4;
} e32_configuration_t;

typedef struct
{
    gpio_num_t M0;
    gpio_num_t M1;
    gpio_num_t AUX;
    gpio_num_t Tx;
    gpio_num_t Rx;
    bool AUX_REVERSE = false;
} e32_pin_t;

typedef struct
{
    uint8_t *buffer;
    size_t size;
} e32_receve_struct_t;

class e32oz
{
private:
    e32_pin_t _pin;
    e32_mode_t _mode;
    QueueHandle_t _internal_queue = NULL;
    QueueHandle_t _external_queue = NULL;
    bool _read_internal;
    uint8_t _uart_num;
    
    int _timing_read(uint8_t * buffer , size_t len, TickType_t xTicksToWait);
    e32_errno_t _wait_for_aux(unsigned long timeout);
    e32_errno_t _set_config(uint8_t command, e32_configuration_t configuration);

    e32_errno_t _begin(uint8_t uart_num, e32_pin_t pin);

public:

    /**
     * @brief Construct a new e32oz object
     *
     */
    e32oz();

    /**
     * @brief 
     * 
     * @param uart_num 
     * @param pin 
     * @return e32_errno_t 
     */
    e32_errno_t begin(uint8_t uart_num, e32_pin_t pin);

    /**
     * @brief Set the mode object
     *
     * @param mode
     * @return e32_errno_t
     */
    e32_errno_t set_mode(e32_mode_t mode);

    /**
     * @brief Get the mode object
     *
     * @return e32_mode_t
     */
    e32_mode_t get_mode() { return this->_mode; }

    /**
     * @brief Set the permanent config object
     *
     * @param configuration
     * @return e32_errno_t
     */
    e32_errno_t set_permanent_config(e32_configuration_t configuration);

    /**
     * @brief Set the temp config object
     *
     * @param configuration
     * @return e32_errno_t
     */
    e32_errno_t set_temp_config(e32_configuration_t configuration);

    /**
     * @brief Get the config object
     *
     * @return e32_configuration_t
     */
    e32_configuration_t get_config();

    /**
     * @brief
     *
     * @param address
     * @param channel
     * @param message
     * @param len
     * @return e32_errno_t
     */
    e32_errno_t send(unsigned int address, uint8_t channel, uint8_t *message, size_t len);


    /**
     * @brief 
     * 
     * @param millis 
     * @return e32_receve_struct_t 
     */
    e32_receve_struct_t receve(TickType_t xTicksToWait);



    /**
     * @brief
     *
     *
     * @return e32_errno_t
     */
    e32_errno_t reset();


    /**
     * @brief Get the queue object
     *
     * @return QueueHandle_t
     */
    QueueHandle_t get_queue();


    /**
     * @brief 
     * 
     * @return gpio_num_t 
     */
    gpio_num_t getM0(){ return this->_pin.M0;}


    /**
     * @brief 
     * 
     * @return gpio_num_t 
     */
    gpio_num_t getM1(){ return this->_pin.M1;}


    /**
     * @brief Get the Aux object
     * 
     * @return gpio_num_t 
     */
    gpio_num_t getAux(){ return this->_pin.AUX;}


    /**
     * @brief 
     * 
     */
    void _execute_read();

    /**
     * @brief 
     * 
     * @param xTicksToWait 
     */
    void flush(TickType_t xTicksToWait);
};

#endif