#include <e32cp.h>

e32cp::e32cp(LoRa_E32 * lora, uint16_t address, uint8_t channel)
{
    this->_aes = new AES(128);
    this->_lora = lora;
    this->_laddress = (uint8_t)(address & 0xff);
    this->_haddress = (uint8_t)((address >> 8) & 0xff);
    this->_channel = channel;
}

void e32cp::attachInterrupt(uart_port_t uart_number,OnE32ReciveMessage callback,e32cp * self)
{
    E32Self = self;
    e32_function_callback = callback;

    uart_isr_register(uart_number,e32_uart_intr_handle, NULL, ESP_INTR_FLAG_LOWMED, e32_handle_console);
    uart_enable_rx_intr(uart_number);
}

void e32cp::begin()
{
    this->_lora->begin();
}

bool e32cp::sleepyWake(uint16_t address, uint8_t channel, String payload)
{
    this->_lora->setMode(MODE_1_WAKE_UP);

    uint8_t addL = (uint8_t)(address & 0xff);
    uint8_t addH = (uint8_t)((address >> 8) & 0xff);

    ResponseStatus rs =  this->_lora->sendFixedMessage(addH,addL,channel,E32_WAKE_COMMAND);

    if(rs.code != ERR_E32_SUCCESS ) return false;

    delay(2000);

    this->_lora->setMode(MODE_0_NORMAL);

    RawResponseContainer CriptedKey = this->_lora->waitForReceiveRawMessage(E32_WAKE_DELAY);

    if(CriptedKey.status.code != ERR_E32_SUCCESS) return false;
    if(CriptedKey.data == NULL) return false;

    uint8_t * oneTimeKey = this->_aes->DecryptECB(CriptedKey.data,CriptedKey.length,E32_PSKEY);

    unsigned int out_len;
    uint8_t * massage = this->_aes->EncryptECB((uint8_t *)payload.c_str(),payload.length(),oneTimeKey,out_len);

    rs = this->_lora->sendFixedMessage(addH,addL,channel,(void*)massage,(uint8_t)out_len);

    if(rs.code == ERR_E32_SUCCESS )
        return true;
    else
        return false;
}

String e32cp::sleepyIsWake()
{
    this->_lora->setMode(MODE_0_NORMAL);
    uint8_t * oneTimeKey = this->OneTimePassword();
    unsigned int out_len;
    uint8_t * CriptOneTimeKey = this->_aes->EncryptECB(oneTimeKey,E32_KEY_LENGTH,E32_PSKEY,out_len);

    ResponseStatus rs =  this->_lora->sendFixedMessage(0,E32_SERVER_ADDRESS,E32_SERVER_CHANNEL,CriptOneTimeKey,(uint8_t)out_len);

    if(rs.code != ERR_E32_SUCCESS ) return String();

    RawResponseContainer CriptMessage = this->_lora->waitForReceiveRawMessage(E32_WAKE_DELAY);

    if(CriptMessage.status.code != ERR_E32_SUCCESS ) return String();

    uint8_t * message = this->_aes->DecryptECB(CriptMessage.data,CriptMessage.length,oneTimeKey);

    return String((char *)message);
}

bool e32cp::sensorSend(String payload)
{
    this->_lora->setMode(MODE_0_NORMAL);
    ResponseStatus rs =  this->_lora->sendFixedMessage(0,E32_SERVER_ADDRESS,E32_SERVER_CHANNEL,E32_HANDUP_COMMAND);
    if(rs.code != ERR_E32_SUCCESS ) return false;

    RawResponseContainer CriptedKey = this->_lora->waitForReceiveRawMessage(E32_WAKE_DELAY);
    if(CriptedKey.status.code != ERR_E32_SUCCESS) return false;
    if(CriptedKey.data == NULL) return false;

    uint8_t * oneTimeKey = this->_aes->DecryptECB(CriptedKey.data,CriptedKey.length,E32_PSKEY);

    unsigned int out_len;
    uint8_t * cript_massage = this->_aes->EncryptECB((uint8_t *)payload.c_str(),payload.length(),oneTimeKey,out_len);

    
    rs = this->_lora->sendFixedMessage(0,E32_SERVER_ADDRESS,E32_SERVER_CHANNEL,cript_massage,(uint8_t)out_len);

    if(rs.code == ERR_E32_SUCCESS )
        return true;
    else
        return false;

}

void e32cp::loop()
{
    if(this->_lora->available())
    {
        ResponseContainer rc =  this->_lora->receiveMessage();

        if(rc.status.code == ERR_E32_SUCCESS)
            e32_function_callback(rc.data);
    }
} 

uint8_t * e32cp::OneTimePassword() 
{ 
    uint8_t * newKey = (uint8_t * )malloc(E32_KEY_LENGTH);

    for(int i = 0 ; i< E32_KEY_LENGTH ; i++)
    {
        newKey[i] = (uint8_t)esp_random();
    }

    return newKey;
}