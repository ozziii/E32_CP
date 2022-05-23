#include "e32oz.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "rom/crc.h"

#define ESP_INTR_FLAG_DEFAULT 0

#define TAG "E32"

static void e32oz_task(void *arg)
{
    e32oz *self = (e32oz *)arg;
    for (;;)
    {
        self->_execute_read();
    }
}

e32oz::e32oz()
{
    this->_read_internal = false;
    this->_internal_queue = xQueueCreate(5, sizeof(e32_receve_struct_t));
    this->_external_queue = xQueueCreate(5, sizeof(e32_receve_struct_t));
}

e32_errno_t e32oz::begin(uint8_t uart_num, e32_pin_t pin)
{
    e32_errno_t err;
    uint8_t count = 3;
    do
    {
        err = this->_begin(uart_num,pin);
        if(err == E32_ERR_SUCCESS)
            return err;
        else
            vTaskDelay(pdMS_TO_TICKS(1000));
        count--;
    } while (count > 0);

    return err;
}

e32_errno_t e32oz::_begin(uint8_t uart_num, e32_pin_t pin)
{
    if (!GPIO_IS_VALID_OUTPUT_GPIO(pin.M0) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(pin.M1) ||
        !GPIO_IS_VALID_GPIO(pin.AUX) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(pin.Tx) ||
        !GPIO_IS_VALID_GPIO(pin.Rx))
        return E32_ERR_INVALID_PIN;

    if (uart_num > 2)
        return E32_ERR_INVALID_UARTNUM;

    this->_uart_num = uart_num;
    this->_pin = pin;

    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};

    if (uart_param_config(this->_uart_num, &uart_config) != ESP_OK)
        return E32_ERR_UART;

    // Set UART pins (using UART0 default pins ie no changes.)
    if (uart_set_pin(this->_uart_num, GPIO_NUM_17, GPIO_NUM_16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK)
        return E32_ERR_UART;

    // Install UART driver, and get the queue.
    if (uart_driver_install(
            this->_uart_num,
            E32_MAX_UARTE_BUFFER_SIZE,
            E32_MAX_UARTE_BUFFER_SIZE,
            0,
            NULL, 0) != ESP_OK)
        return E32_ERR_UART;

    pinMode(this->_pin.M0, OUTPUT);
    pinMode(this->_pin.M1, OUTPUT);
    digitalWrite(this->_pin.M0, 0);
    digitalWrite(this->_pin.M1, 0);
    this->_mode = E32_MODE_NORMAL;

    pinMode(this->_pin.AUX, INPUT);

    gpio_int_type_t interr_type;

    if (this->_pin.AUX_REVERSE)
        interr_type = GPIO_INTR_POSEDGE;
    else
        interr_type = GPIO_INTR_NEGEDGE;

    ESP_ERROR_CHECK(gpio_set_intr_type(this->_pin.AUX, interr_type));
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT));

    this->_internal_queue = xQueueCreate(3, sizeof(e32_receve_struct_t));
    if (this->_internal_queue == NULL)
    {
        return E32_ERR_QUEUE_CREATE;
    }
    this->_external_queue = xQueueCreate(3, sizeof(e32_receve_struct_t));
    if (this->_internal_queue == NULL)
    {
        return E32_ERR_QUEUE_CREATE;
    }

    xTaskCreate(e32oz_task, "e32oz_task", 2048, this, 10, NULL);

    return E32_ERR_SUCCESS;
}

e32_errno_t e32oz::set_mode(e32_mode_t mode)
{
    if (mode == this->_mode)
        return E32_ERR_SUCCESS;

    vTaskDelay(pdMS_TO_TICKS(20));

    switch (mode)
    {
    case E32_MODE_NORMAL:
        digitalWrite(this->_pin.M0, 0);
        digitalWrite(this->_pin.M1, 0);
        break;
    case E32_MODE_WAKE_UP:
        digitalWrite(this->_pin.M0, 1);
        digitalWrite(this->_pin.M1, 0);
        break;
    case E32_MODE_POWER_SAVING:
        digitalWrite(this->_pin.M0, 0);
        digitalWrite(this->_pin.M1, 1);
        break;
    case E32_MODE_SLEEP:
        digitalWrite(this->_pin.M0, 1);
        digitalWrite(this->_pin.M1, 1);
        break;
    }
    vTaskDelay(pdMS_TO_TICKS(20));

    e32_errno_t err = this->_wait_for_aux(1000);

    if (err == E32_ERR_SUCCESS)
        this->_mode = mode;

    return err;
}

e32_errno_t e32oz::set_permanent_config(e32_configuration_t configuration)
{
    return this->_set_config(E32_COMMAND_C0, configuration);
}

e32_errno_t e32oz::set_temp_config(e32_configuration_t configuration)
{
    return this->_set_config(E32_COMMAND_C2, configuration);
}

e32_configuration_t e32oz::get_config()
{
    e32_configuration_t ret;
    e32_mode_t prev_mode = this->_mode;

    ret.error = this->set_mode(E32_MODE_SLEEP);
    if (ret.error != E32_ERR_SUCCESS)
    {
        return ret;
    }

    this->_read_internal = true;

    uint8_t command =  E32_COMMAND_C1;
    uart_write_bytes(this->_uart_num, &command,1);
    uart_write_bytes(this->_uart_num, &command,1);
    uart_write_bytes(this->_uart_num, &command,1);


    vTaskDelay(pdMS_TO_TICKS(100));

    e32_receve_struct_t message;

    if (!xQueueReceive(this->_internal_queue, &message, pdMS_TO_TICKS(1000)))
    {
        ret.error = E32_ERR_COMUNICATION;
        this->_read_internal = false;
        return ret;
    }

    if (message.size < 6)
    {
        ret.error = E32_ERR_COMUNICATION;
        free(message.buffer);
        this->_read_internal = false;
        return ret;
    }

    this->_read_internal = false;

    ret.error = this->_wait_for_aux(1000);
    if (ret.error != E32_ERR_SUCCESS)
    {
        return ret;
    }

    ret.error = this->set_mode(prev_mode);
    if (ret.error != E32_ERR_SUCCESS)
    {
        return ret;
    }

    uint8_t bite0 = *(message.buffer + message.size - 6);
    uint8_t bite1 = *(message.buffer + message.size - 5);
    uint8_t bite2 = *(message.buffer + message.size - 4);
    uint8_t bite3 = *(message.buffer + message.size - 3);
    uint8_t bite4 = *(message.buffer + message.size - 2);
    uint8_t bite5 = *(message.buffer + message.size - 1);

    ESP_LOGV(TAG, "CONF -> %x:%x:%x:%x:%x:%x",
             bite0,
             bite1,
             bite2,
             bite3,
             bite4,
             bite5);

    ret.address_H = bite1;
    ret.address_L = bite2;

    ret.uart_parity = (e32_uart_parity_t)(bite3 & E32_BITMASK_UARTPARITY);
    ret.uart_baud_rate = (e32_uart_baud_rate_t)(bite3 & E32_BITMASK_UARTBAUD);
    ret.air_data_rate = (e32_air_data_rate_t)(bite3 & E32_BITMASK_AIRDATARATE);

    ret.channel = bite4 & E32_BITMASK_CHANNEL;

    ret.fixed_mode = (e32_fixed_mode_t)(bite5 & E32_BITMASK_FIXEDMODE);
    ret.io_driver_mode = (e32_io_driver_mode_t)(bite5 & E32_BITMASK_IODRIVER);
    ret.wake_up_time = (e32_wake_up_time_t)(bite5 & E32_BITMASK_WAKEUP);
    ret.fec_enabled = (e32_fec_enabled_t)(bite5 & E32_BITMASK_FEC);
    ret.transmission_power = (e32_transmission_power_t)(bite5 & E32_BITMASK_POWER);

    ret.error = E32_ERR_SUCCESS;

    return ret;
}

e32_errno_t e32oz::send(unsigned int address, uint8_t channel, uint8_t *message, size_t len)
{
    if (len > E32_MAX_MESSAGE_SIZE)
        return E32_ERR_MESSAGE_SIZE;

    if (this->_mode == E32_MODE_POWER_SAVING || this->_mode == E32_MODE_SLEEP)
        return E32_ERR_WRONG_MODE;

    uint8_t addL = (uint8_t)(address & 0xff);
    uint8_t addH = (uint8_t)((address >> 8) & 0xff);
    uint8_t crc = crc8_le(0, (const uint8_t *)message, len);

    uart_write_bytes(this->_uart_num, &addH,1);
    uart_write_bytes(this->_uart_num, &addL,1);
    uart_write_bytes(this->_uart_num, &channel,1);
    uart_write_bytes(this->_uart_num, message, len);
    uart_write_bytes(this->_uart_num, &crc,1);

    auto ret = _wait_for_aux(1000);

    return ret;
}

e32_receve_struct_t e32oz::receve(TickType_t xTicksToWait)
{
    this->_read_internal = true;
    e32_receve_struct_t message;
    message.buffer = NULL;
    message.size = 0;

    if (xQueueReceive(this->_internal_queue, &message,  xTicksToWait))
    {

        if (message.size > 1)
        {
            message.size--;
            uint8_t crc = crc8_le(0, message.buffer, message.size); // GET CRC VALUE
            uint8_t Rcrc = *(message.buffer + message.size);

            if (Rcrc == crc)
            {
                this->_read_internal = false;
                return message;
            }

            ESP_LOGD(TAG, "CRC ERROR");
        }

        if (message.buffer != NULL)
            free(message.buffer);
        message.buffer = NULL;
        message.size = 0;
    }

    this->_read_internal = false;
    return message;
}

QueueHandle_t e32oz::get_queue()
{
    return this->_external_queue;
}

e32_errno_t e32oz::_wait_for_aux(unsigned long timeout)
{
    unsigned long start = millis();

    while (gpio_get_level(this->_pin.AUX) == this->_pin.AUX_REVERSE) // TODO control aux
    {
        vTaskDelay(E32_AUX_SLEEP_TIMEOUT);
        if (millis() - start > timeout) // TODO control loop error
        {
            return E32_ERR_TIMEOUT;
        }
    }
    return E32_ERR_SUCCESS;
}

e32_errno_t e32oz::_set_config(uint8_t command, e32_configuration_t configuration)
{
    e32_mode_t prev_mode = this->_mode;
    e32_errno_t err = this->set_mode(E32_MODE_SLEEP);
    if (err != E32_ERR_SUCCESS)
        return err;

    this->_read_internal = true;


    uint8_t command_c1 =  E32_COMMAND_C1;
    uart_write_bytes(this->_uart_num, &command_c1,1);
    uart_write_bytes(this->_uart_num, &command_c1,1);
    uart_write_bytes(this->_uart_num, &command_c1,1);

    e32_receve_struct_t message;
    if (!xQueueReceive(this->_internal_queue, &message, pdMS_TO_TICKS(1000)))
    {
        this->_read_internal = false;
        return E32_ERR_COMUNICATION;
    }
    free(message.buffer);


    this->_read_internal = false;

    uint8_t data[6];

    data[0] = command;
    data[1] = configuration.address_H;
    data[2] = configuration.address_L;
    data[3] = configuration.uart_parity +
              configuration.uart_baud_rate +
              configuration.air_data_rate;
    data[4] = configuration.channel;
    data[5] = configuration.fixed_mode +
              configuration.io_driver_mode +
              configuration.wake_up_time +
              configuration.fec_enabled +
              configuration.transmission_power;

    ESP_LOGV(TAG, "SEND: CMD[%X] ADDH[%X] ADDL[%X] SPEED[%X] CH[%X] OPT[%X]",
             data[0],
             data[1],
             data[2],
             data[3],
             data[4],
             data[5]);

    uart_write_bytes(this->_uart_num, (const uint8_t *)data, 6);

    err = this->_wait_for_aux(1000);
    if (err != E32_ERR_SUCCESS)
        return err;

    err = this->set_mode(prev_mode);
    if (err != E32_ERR_SUCCESS)
        return err;

    return E32_ERR_SUCCESS;
}

int e32oz::_timing_read(uint8_t *buffer, size_t len, TickType_t xTicksToWait)
{
    uint8_t *buffer_pointer = buffer;
    size_t size = 0;
    uint8_t c;
    if (uart_read_bytes(this->_uart_num, &c,1, xTicksToWait))
    {
        do
        {
            (*buffer_pointer) = c;
            //ESP_LOGV("R","-->[%X]",c);
            buffer_pointer++;
            size++;
        } while (uart_read_bytes(this->_uart_num, &c,1,10) && size < len);

        return size;
    }
    else
    {
        return E32_ERR_TIMEOUT;
    }
}

void e32oz::_execute_read()
{
    e32_receve_struct_t message;
    message.buffer = (uint8_t *)malloc(sizeof(uint8_t) * E32_MAX_MESSAGE_SIZE);

    if(message.buffer == NULL)  // NO MEMORY ERROR
    {
        vTaskDelay(pdMS_TO_TICKS(300));
        return;
    }

    int len = this->_timing_read(message.buffer, E32_MAX_MESSAGE_SIZE, portMAX_DELAY);

    if (len > 0)
    {
        message.size = len;

        if (this->_read_internal)
        {
            if (this->_internal_queue != NULL)
            {
                xQueueSend(this->_internal_queue, &message, 10);
                return;
            }
        }
        else
        {
            if (message.size > 1)
            {
                message.size--;
                uint8_t crc = crc8_le(0, message.buffer, message.size); // CALC CRC VALUE
                uint8_t Rcrc = *(message.buffer + message.size);        // GET CRC VALUE FROM DATA

                if (crc == Rcrc && this->_external_queue != NULL)
                {
                    xQueueSend(this->_external_queue, &message, 10);
                    return;
                }
            }
        }
    }

    free(message.buffer);
}

void e32oz::flush(TickType_t xTicksToWait)
{
    e32_receve_struct_t message;
    while (xQueueReceive(this->_internal_queue, &message,  xTicksToWait))
    {
        if(message.buffer  != NULL)
            free(message.buffer);
    }
    

    while (xQueueReceive(this->_external_queue, &message,  xTicksToWait))
    {
        if(message.buffer  != NULL)
            free(message.buffer);
    }
}