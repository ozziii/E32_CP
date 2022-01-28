#include <e32cp.h>
#include <vector>


#define MAX_MUTEX_DELAY_LOOP_ms    1000
#define MAX_MUTEX_DELAY_ROUTINE_ms 5000

std::vector<String> splitString(const char *init, char c)
{
    std::vector<String> ret;

    String src = init;

    int i2 = 0;
    int i = src.indexOf(c);

    while (i > 0)
    {
        String item = src.substring(i2, i);
        if (item.length() != 0 && item.indexOf(c) < 0)
        {
            ret.push_back(item);
        }
        i2 = i + 1;
        i = src.indexOf(c, i2);
    }

    String item = src.substring(i2, src.length());
    if (item.length() != 0 && item.indexOf(c) < 0)
    {
        ret.push_back(item);
    }

    return ret;
}

e32cp::e32cp()
{
    this->uar_mutex = xSemaphoreCreateMutex();
}

bool e32cp::begin(e32cp_config_t config)
{
    if(config.lora = nullptr)
        return false;

    if (config.key_length != 32 && config.key_length != 24 && config.key_length != 16)
        return false;

    this->_pre_shared_key = config.pre_shared_key;
    this->_key_length = config.key_length;
    this->_lora =  config.lora;
    this->_laddress = (uint8_t)( config.address & 0xff);
    this->_haddress = (uint8_t)(( config.address >> 8) & 0xff);
    this->_channel =  config.channel;
    this->_bootloader_random =  config.bootloader_random;
    
    
    return this->_lora->begin();
}

bool e32cp::config()
{
    if(xSemaphoreTake(this->uar_mutex,pdMS_TO_TICKS(MAX_MUTEX_DELAY_ROUTINE_ms)) == pdFALSE ) return false;

    ResponseStructContainer c;
    c = this->_lora->getConfiguration();
    // It's important get configuration pointer before all other operation
    if (c.status.code != ERR_E32_SUCCESS || c.data == NULL)
    {
        c.close();
        xSemaphoreGive(this->uar_mutex);
        return false;
    }

    Configuration configuration = *(Configuration *)c.data;
    configuration.ADDH = this->_haddress;
    configuration.ADDL = this->_laddress;
    configuration.CHAN = this->_channel;
    configuration.OPTION.wirelessWakeupTime = WAKE_UP_1750;
    configuration.SPED.uartParity = MODE_00_8N1;
    configuration.SPED.uartBaudRate = UART_BPS_9600;
    configuration.SPED.airDataRate = AIR_DATA_RATE_010_24;
    configuration.OPTION.fixedTransmission = FT_FIXED_TRANSMISSION;
    configuration.OPTION.fec = FEC_1_ON;
    configuration.OPTION.transmissionPower = POWER_20;
    ResponseStatus rs = this->_lora->setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);

    c.close();
    xSemaphoreGive(this->uar_mutex);

    if (rs.code == ERR_E32_SUCCESS)
        return true;
    else
        return false;
}

bool e32cp::sleepy_wake(uint16_t address, uint8_t channel, String payload)
{
    if(xSemaphoreTake(this->uar_mutex,pdMS_TO_TICKS(MAX_MUTEX_DELAY_ROUTINE_ms)) == pdFALSE ) return false;

    Status mode = this->_lora->setMode(MODE_1_WAKE_UP);

    if (mode != ERR_E32_SUCCESS)
    {
        xSemaphoreGive(this->uar_mutex);
        E32CP_LOGD("Error in set Mode WAKE UP \r\n");
        return false;
    }

    uint8_t addL = (uint8_t)(address & 0xff);
    uint8_t addH = (uint8_t)((address >> 8) & 0xff);

    ResponseStatus rs = this->_lora->sendFixedMessage(addH, addL, channel, E32_WAKE_COMMAND);

    if (rs.code != ERR_E32_SUCCESS)
    {
        xSemaphoreGive(this->uar_mutex);
        E32CP_LOGD("Error in send Wake: %s \n", rs.getResponseDescription().c_str());
        return false;
    }

    E32CP_LOGD("Send Wake: : H:%u,L:%u,C:%u \n", addH, addL, channel);

    delay(2000);

    mode = this->_lora->setMode(MODE_0_NORMAL);

    if (mode != ERR_E32_SUCCESS)
    {
        xSemaphoreGive(this->uar_mutex);
        E32CP_LOGD("Error in set Mode NORMAL \r\n");
        return false;
    }

    RawResponseContainer CriptedKey = this->_lora->waitForReceiveRawMessage(E32_WAKE_DELAY);

    if (CriptedKey.status.code != ERR_E32_SUCCESS)
    {
        xSemaphoreGive(this->uar_mutex);
        E32CP_LOGD("Error in recieve key  : %s \r\n", CriptedKey.status.getResponseDescription().c_str());
        return false;
    }


    if (CriptedKey.data == NULL)
    {
        xSemaphoreGive(this->uar_mutex);
        return false;
    }

    uint8_t *oneTimeKey = oz_aes::decrypt_CBC(CriptedKey.data, CriptedKey.length, this->_pre_shared_key, this->_key_length);

    CriptedKey.close();

    unsigned int out_len;

    uint8_t *message = oz_aes::encrypt_CBC(payload, oneTimeKey, CriptedKey.length, out_len);

    rs = this->_lora->sendFixedMessage(addH, addL, channel, (void *)message, (uint8_t)out_len);

    free(message);

    xSemaphoreGive(this->uar_mutex);

    if (rs.code == ERR_E32_SUCCESS)
    {
        E32CP_LOGD("Send Message : H:%u,L:%u,C:%u \n", addH, addL, channel);
        
        return true;
    }
    else
    {
        E32CP_LOGD("Error in send Mesage  : H:%u,L:%u,C:%u \n", addH, addL, channel);
        return false;
    }
}

String e32cp::sleepy_is_wake()
{
    if(xSemaphoreTake(this->uar_mutex,pdMS_TO_TICKS(MAX_MUTEX_DELAY_ROUTINE_ms)) == pdFALSE ) return String();

    this->_lora->setMode(MODE_0_NORMAL);
    uint8_t *oneTimeKey = this->_one_time_password();
    unsigned int out_len;
    uint8_t *CriptOneTimeKey = oz_aes::encrypt_CBC(oneTimeKey, this->_key_length, (uint8_t *)this->_pre_shared_key, this->_key_length, out_len);

    ResponseStatus rs = this->_lora->sendFixedMessage(0, E32_SERVER_ADDRESS, E32_SERVER_CHANNEL, CriptOneTimeKey, (uint8_t)out_len);

    if (rs.code != ERR_E32_SUCCESS)
    {
        xSemaphoreGive(this->uar_mutex);
        return String();
    }

    RawResponseContainer CriptMessage = this->_lora->waitForReceiveRawMessage(E32_WAKE_DELAY);

    xSemaphoreGive(this->uar_mutex);

    if (CriptMessage.status.code != ERR_E32_SUCCESS)
        return String();

    uint8_t *message = oz_aes::decrypt_CBC(CriptMessage.data, CriptMessage.length, oneTimeKey, this->_key_length);

    free(oneTimeKey);
    free(CriptOneTimeKey);
    CriptMessage.close();

    return String((char *)message);
}

bool e32cp::sensor_send(String payload)
{
    if(xSemaphoreTake(this->uar_mutex,pdMS_TO_TICKS(MAX_MUTEX_DELAY_ROUTINE_ms)) == pdFALSE ) return false;

    this->_lora->setMode(MODE_0_NORMAL);

    String hand_up_message = E32_HANDUP_COMMAND;
    hand_up_message += E32_HANDUP_SEPARATOR_CHAR;
    hand_up_message += String(this->_laddress);

    ResponseStatus rs = this->_lora->sendFixedMessage(0, E32_SERVER_ADDRESS, E32_SERVER_CHANNEL, hand_up_message);

    if (rs.code != ERR_E32_SUCCESS)
    {
        E32CP_LOGD("Send HandUp : [%s]  \r\n", rs.getResponseDescription().c_str());
        xSemaphoreGive(this->uar_mutex);
        return false;
    }

    RawResponseContainer CriptedKey = this->_lora->waitForReceiveRawMessage(E32_WAKE_DELAY);
    

    if (CriptedKey.status.code != ERR_E32_SUCCESS)
    {
        E32CP_LOGD("Error timeout get key : [%s] \r\n", CriptedKey.status.getResponseDescription().c_str());
        xSemaphoreGive(this->uar_mutex);
        return false;
    }

    if (CriptedKey.data == NULL)
    {  
        xSemaphoreGive(this->uar_mutex);
        return false;
    }

    uint8_t *oneTimeKey = oz_aes::decrypt_CBC(CriptedKey.data, CriptedKey.length, (uint8_t *)this->_pre_shared_key, this->_key_length);

    unsigned int out_len;
    uint8_t *cript_massage = oz_aes::encrypt_CBC(payload, oneTimeKey, this->_key_length, out_len);

    rs = this->_lora->sendFixedMessage(0, E32_SERVER_ADDRESS, E32_SERVER_CHANNEL, cript_massage, (uint8_t)out_len);

    xSemaphoreGive(this->uar_mutex);

    if (rs.code == ERR_E32_SUCCESS)
    {
        E32CP_LOGD("Send Messge success : [%s] \r\n", rs.getResponseDescription().c_str());
        return true;
    }
    else
    {
        E32CP_LOGD("Error in send Message : [%s] ", rs.getResponseDescription().c_str());
        return false;
    }
}

String e32cp::_server_recieve()
{

    ResponseContainer rc = this->_lora->receiveMessage();

    if (rc.status.code != ERR_E32_SUCCESS)
        return String();

    std::vector<String> command = splitString(rc.data.c_str(), E32_HANDUP_SEPARATOR_CHAR);

    if (command.size() < 2)
        return String();

    if (command[0].compareTo(E32_HANDUP_COMMAND))
    {

        byte clientAddress = atoi(command[1].c_str());

        this->_lora->setMode(MODE_0_NORMAL);

        uint8_t *oneTimeKey = this->_one_time_password();

        unsigned int out_len;

        uint8_t *CriptOneTimeKey = oz_aes::encrypt_CBC(oneTimeKey, this->_key_length, (uint8_t *)this->_pre_shared_key, this->_key_length, out_len);

        ResponseStatus rs = this->_lora->sendFixedMessage(0, clientAddress, E32_SERVER_CHANNEL, CriptOneTimeKey, (uint8_t)out_len);

        if (rs.code != ERR_E32_SUCCESS)
        {
            return String();
        }

        RawResponseContainer CriptMessage = this->_lora->waitForReceiveRawMessage(E32_WAKE_DELAY);

        if (CriptMessage.status.code != ERR_E32_SUCCESS)
            return String();

        uint8_t *message = oz_aes::decrypt_CBC(CriptMessage.data, CriptMessage.length, oneTimeKey, this->_key_length);


        free(oneTimeKey);
        free(CriptOneTimeKey);
        CriptMessage.close();

        return String((char *)message);
    }

    return String();
}

String e32cp::available()
{
    if(xSemaphoreTake(this->uar_mutex,pdMS_TO_TICKS(MAX_MUTEX_DELAY_LOOP_ms)) == pdFALSE ) return String();
    
    if(this->_lora->available() <= 0) return String();
   
    String ret = this->_server_recieve();

    xSemaphoreGive(this->uar_mutex);

    return String();
}

uint8_t *e32cp::_one_time_password()
{
    uint8_t *newKey = (uint8_t *)malloc(this->_key_length);

    if (this->_bootloader_random)
        bootloader_random_enable();

    bootloader_fill_random((void *)newKey, this->_key_length);

    if (this->_bootloader_random)
        bootloader_random_disable();

    return newKey;
}