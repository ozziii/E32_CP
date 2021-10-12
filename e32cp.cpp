#include <e32cp.h>
#include <vector>

static uint8_t E32_PSKEY[E32_KEY_LENGTH] =
    {
        0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88,
        0x99, 0xAA, 0xBB, 0xCC,
        0xDD, 0xEE, 0xA2, 0xB3};

static std::vector<String> splitString(const char *init, char c)
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

e32cp::e32cp(LoRa_E32 *lora, uint16_t address, uint8_t channel, bool bootloader_random)
{
    this->_aes = new AES(128);
    this->_lora = lora;
    this->_laddress = (uint8_t)(address & 0xff);
    this->_haddress = (uint8_t)((address >> 8) & 0xff);
    this->_channel = channel;
    this->_bootloader_random = bootloader_random;
    this->e32cp_stop_lissen = false;
}

void e32cp::attachInterrupt(OnE32ReciveMessage callback)
{
    e32_function_callback = callback;
}

bool e32cp::begin()
{
    return this->_lora->begin();
}

bool e32cp::config()
{
    ResponseStructContainer c;
    c = this->_lora->getConfiguration();
    // It's important get configuration pointer before all other operation
    if (c.status.code != ERR_E32_SUCCESS || c.data == NULL)
    {
        c.close();
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

    if (rs.code == ERR_E32_SUCCESS)
        return true;
    else
        return false;
}

bool e32cp::sleepyWake(uint16_t address, uint8_t channel, String payload)
{
    this->e32cp_stop_lissen = true;

    Status mode = this->_lora->setMode(MODE_1_WAKE_UP);

    if (mode != ERR_E32_SUCCESS)
    {
        this->e32cp_stop_lissen = false;
        E32CP_LOGD("Error in set Mode WAKE UP \r\n");
        return false;
    }


    uint8_t addL = (uint8_t)(address & 0xff);
    uint8_t addH = (uint8_t)((address >> 8) & 0xff);

    ResponseStatus rs = this->_lora->sendFixedMessage(addH, addL, channel, E32_WAKE_COMMAND);

    if (rs.code != ERR_E32_SUCCESS)
    {
        this->e32cp_stop_lissen = false;
        E32CP_LOGD("Error in send Wake: %s \n", rs.getResponseDescription().c_str());
        return false;
    }

    E32CP_LOGD("Send Wake: : H:%u,L:%u,C:%u \n", addH, addL, channel);

    delay(2000);

    mode = this->_lora->setMode(MODE_0_NORMAL);

    if (mode != ERR_E32_SUCCESS)
    {
        this->e32cp_stop_lissen = false;
        E32CP_LOGD("Error in set Mode NORMAL \r\n");
        return false;
    }


    RawResponseContainer CriptedKey = this->_lora->waitForReceiveRawMessage(E32_WAKE_DELAY);

    if (CriptedKey.status.code != ERR_E32_SUCCESS)
    {
        E32CP_LOGD("Error in recieve key  : %s \r\n", CriptedKey.status.getResponseDescription().c_str());
        return false;
    }

    this->e32cp_stop_lissen = false;

    if (CriptedKey.data == NULL)
    {
        return false;
    }

    uint8_t *oneTimeKey = this->_aes->DecryptECB(CriptedKey.data, CriptedKey.length, E32_PSKEY);

    CriptedKey.close();

    unsigned int out_len;
    uint8_t *message = this->_aes->EncryptECB((uint8_t *)payload.c_str(), payload.length(), oneTimeKey, out_len);

    rs = this->_lora->sendFixedMessage(addH, addL, channel, (void *)message, (uint8_t)out_len);

    free(message);

    if (rs.code == ERR_E32_SUCCESS)
    {
        return true;
        E32CP_LOGD("Send Message : H:%u,L:%u,C:%u \n", addH, addL, channel);
    }
    else
    {
        E32CP_LOGD("Error in send Mesage  : H:%u,L:%u,C:%u \n", addH, addL, channel);
        return false;
    }
}

String e32cp::sleepyIsWake()
{
    this->_lora->setMode(MODE_0_NORMAL);
    uint8_t *oneTimeKey = this->OneTimePassword();
    unsigned int out_len;
    uint8_t *CriptOneTimeKey = this->_aes->EncryptECB(oneTimeKey, E32_KEY_LENGTH, E32_PSKEY, out_len);

    this->e32cp_stop_lissen = true;

    ResponseStatus rs = this->_lora->sendFixedMessage(0, E32_SERVER_ADDRESS, E32_SERVER_CHANNEL, CriptOneTimeKey, (uint8_t)out_len);

    if (rs.code != ERR_E32_SUCCESS)
    {
        this->e32cp_stop_lissen = false;
        return String();
    }

    RawResponseContainer CriptMessage = this->_lora->waitForReceiveRawMessage(E32_WAKE_DELAY);

    this->e32cp_stop_lissen = false;

    if (CriptMessage.status.code != ERR_E32_SUCCESS)
        return String();

    uint8_t *message = this->_aes->DecryptECB(CriptMessage.data, CriptMessage.length, oneTimeKey);

    free(oneTimeKey);
    free(CriptOneTimeKey);
    CriptMessage.close();

    return String((char *)message);
}

bool e32cp::sensorSend(String payload)
{
    this->_lora->setMode(MODE_0_NORMAL);

    String hand_up_message = E32_HANDUP_COMMAND;
    hand_up_message += E32_HANDUP_SEPARATOR_CHAR; 
    hand_up_message += String(this->_laddress);

    this->e32cp_stop_lissen = true;

    ResponseStatus rs = this->_lora->sendFixedMessage(0, E32_SERVER_ADDRESS, E32_SERVER_CHANNEL, hand_up_message);

    if (rs.code != ERR_E32_SUCCESS)
    {
        this->e32cp_stop_lissen = false;
        E32CP_LOGD("Send HandUp : [%s]  \r\n", rs.getResponseDescription().c_str());
        return false;
    }

    RawResponseContainer CriptedKey = this->_lora->waitForReceiveRawMessage(E32_WAKE_DELAY);

    this->e32cp_stop_lissen = false;

    if (CriptedKey.status.code != ERR_E32_SUCCESS)
    {
        E32CP_LOGD("Error timeout get key : [%s] \r\n", CriptedKey.status.getResponseDescription().c_str());
        return false;
    }

    if (CriptedKey.data == NULL)
        return false;

    uint8_t *oneTimeKey = this->_aes->DecryptECB(CriptedKey.data, CriptedKey.length, E32_PSKEY);

    unsigned int out_len;
    uint8_t *cript_massage = this->_aes->EncryptECB((uint8_t *)payload.c_str(), payload.length(), oneTimeKey, out_len);

    rs = this->_lora->sendFixedMessage(0, E32_SERVER_ADDRESS, E32_SERVER_CHANNEL, cript_massage, (uint8_t)out_len);

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

String e32cp::ServerRecieve()
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

        uint8_t *oneTimeKey = this->OneTimePassword();

        unsigned int out_len;

        uint8_t *CriptOneTimeKey = this->_aes->EncryptECB(oneTimeKey, E32_KEY_LENGTH, E32_PSKEY, out_len);

        this->e32cp_stop_lissen = true;

        ResponseStatus rs = this->_lora->sendFixedMessage(0, clientAddress, E32_SERVER_CHANNEL, CriptOneTimeKey, (uint8_t)out_len);

        if (rs.code != ERR_E32_SUCCESS)
        {
            this->e32cp_stop_lissen = false;
            return String();
        }

        RawResponseContainer CriptMessage = this->_lora->waitForReceiveRawMessage(E32_WAKE_DELAY);

        this->e32cp_stop_lissen = false;

        if (CriptMessage.status.code != ERR_E32_SUCCESS)
            return String();

        uint8_t *message = this->_aes->DecryptECB(CriptMessage.data, CriptMessage.length, oneTimeKey);

        free(oneTimeKey);
        free(CriptOneTimeKey);
        CriptMessage.close();

        return String((char *)message);
    }

    return String();
}

void e32cp::loop()
{
    if (this->available())
    {
        String result = this->ServerRecieve();
        if (result.length() > 0)
            e32_function_callback(result);
    }
}

bool e32cp::available()
{
    if (this->e32cp_stop_lissen)
        return false;

    return (this->_lora->available() > 0);
}

uint8_t *e32cp::OneTimePassword()
{
    uint8_t *newKey = (uint8_t *)malloc(E32_KEY_LENGTH);

    if (this->_bootloader_random)
        bootloader_random_enable();

    bootloader_fill_random((void *)newKey, E32_KEY_LENGTH);

    if (this->_bootloader_random)
        bootloader_random_disable();

    /*
    for(int i = 0 ; i< E32_KEY_LENGTH ; i++)
    {
        newKey[i] = (uint8_t)esp_random();
    }
    */

    return newKey;
}
