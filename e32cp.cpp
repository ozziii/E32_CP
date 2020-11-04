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

    if(rs.code != SUCCESS ) return false;

    this->_lora->setMode(MODE_0_NORMAL);

    ResponseContainer CriptedKey = this->_lora->waitForReceiveMessage(E32_WAKE_DELAY);

    if(CriptedKey.status.code != SUCCESS) return false;
    if(CriptedKey.data == NULL) return false;

    String oneTimeKey = this->decript(CriptedKey.data,E32_PSKEY);

    String massage = this->encript(payload,oneTimeKey);

    rs = this->_lora->sendFixedMessage(addH,addL,channel,massage);

    if(rs.code == SUCCESS )
        return true;
    else
        return false;
}

String e32cp::sleepyIsWake()
{
    this->_lora->setMode(MODE_0_NORMAL);
    String oneTimeKey = this->OneTimePassword();
    String CriptOneTimeKey = this->encript(oneTimeKey,E32_PSKEY);
    ResponseStatus rs =  this->_lora->sendFixedMessage(0,E32_SERVER_ADDRESS,E32_SERVER_CHANNEL,CriptOneTimeKey);

    if(rs.code != SUCCESS ) return String();

    ResponseContainer CriptMessage = this->_lora->waitForReceiveMessage(E32_WAKE_DELAY);

    if(CriptMessage.status.code != SUCCESS ) return String();

    return decript(CriptMessage.data,oneTimeKey);
}

bool e32cp::sensorSend(String payload)
{
    this->_lora->setMode(MODE_0_NORMAL);
    ResponseStatus rs =  this->_lora->sendFixedMessage(0,E32_SERVER_ADDRESS,E32_SERVER_CHANNEL,E32_HANDUP_COMMAND);
    if(rs.code != SUCCESS ) return false;

    ResponseContainer CriptedKey = this->_lora->waitForReceiveMessage(E32_WAKE_DELAY);
    if(CriptedKey.status.code != SUCCESS) return false;
    if(CriptedKey.data == NULL) return false;

    String oneTimeKey = this->decript(CriptedKey.data,E32_PSKEY);
    String massage = this->encript(payload,oneTimeKey);
    rs = this->_lora->sendFixedMessage(0,E32_SERVER_ADDRESS,E32_SERVER_CHANNEL,massage);

    if(rs.code == SUCCESS )
        return true;
    else
        return false;

}


void e32cp::loop()
{
    if(this->_lora->available())
    {
        ResponseContainer rc =  this->_lora->receiveMessage();

        if(rc.status.code == SUCCESS)
            e32_function_callback(rc.data);
    }
} 

String e32cp::OneTimePassword() 
{ 
    char newKey[16];

    for(int i = 0 ; i< 16 ; i++)
    {
        newKey[i] = esp_random();
    }

    return String(newKey);
}

String e32cp::decript(String data, String Key)
{ 
    char* result = _aes->DecryptECB((char*)data.c_str(),data.length(),(char*)Key.c_str());
    return String(result);
}

String e32cp::encript(String data, String Key)
{ 
    unsigned int out_len = 0;
    char * result =  _aes->EncryptECB((char*)data.c_str(),data.length(),(char*)Key.c_str(),out_len);
    return String(result);
}