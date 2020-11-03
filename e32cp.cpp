#include <e32cp.h>

e32cp::e32cp(LoRa_E32 * lora, uint16_t address, uint8_t channel)
{
    this->_aes = new AES(128);
    this->_lora = lora;
    this->_laddress = (uint8_t)(address & 0xff);
    this->_haddress = (uint8_t)((address >> 8) & 0xff);
    this->_channel = channel;
}

e32cp::e32cp(LoRa_E32  * lora, OnE32ReciveMessage callback)
{
    this->_aes = new AES(128);
    this->_laddress = E32_SERVER_ADDRESS;
    this->_haddress = 0;
    this->_channel = E32_SERVER_CHANNEL;
    this->_lora = lora;
    this->_callback = callback;
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
    unsigned char* udata = reinterpret_cast<unsigned char*>(const_cast<char*>(data.c_str()));
    unsigned char* ukey = reinterpret_cast<unsigned char*>(const_cast<char*>(Key.c_str()));
    unsigned char * result =  _aes->DecryptECB(udata,data.length(),ukey);
    return String(reinterpret_cast< char const* >(result));
}


String e32cp::encript(String data, String Key)
{ 
    unsigned char* udata = reinterpret_cast<unsigned char*>(const_cast<char*>(data.c_str()));
    unsigned char* ukey = reinterpret_cast<unsigned char*>(const_cast<char*>(Key.c_str()));
    unsigned int out_len = 0;
    unsigned char * result =  _aes->EncryptECB(udata,data.length(),ukey,out_len);
    return String(reinterpret_cast< char const* >(result));
}