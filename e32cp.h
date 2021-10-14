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
    #define E32CP_DEBUG_PRINTER Serial
	#define E32CP_LOGD(format,...) { E32CP_DEBUG_PRINTER.printf(format,##__VA_ARGS__); }
#else
	#define  E32CP_LOGD(format,...) {}
#endif


// SERVER ADDRESS
#define E32_SERVER_ADDRESS 0
#define E32_SERVER_CHANNEL 3
#define E32_WAKE_COMMAND "wake"
#define E32_HANDUP_COMMAND "hup"
#define E32_HANDUP_SEPARATOR_CHAR 47
#define E32_WAKE_DELAY 4000
#define E32_KEY_LENGTH 16

typedef std::function<void(String Payload)> OnE32ReciveMessage;

class e32cp
{
public:
     /**
         * Client CTOR
         * 
         * 
         * 
         */
     e32cp(LoRa_E32 *lora, uint16_t address = E32_SERVER_ADDRESS, uint8_t channel = E32_SERVER_CHANNEL, bool bootloader_random = false);

     /**
         *   MAKE ASYCRONUS URAT FUNCTION ONLY FOR HARDWARE SERIAL
         * 
         * 
         */
     void attachInterrupt(OnE32ReciveMessage callback);

     /**
         * 
         */
     bool begin();

     /**
         * 
         * 
         * 
         */
     bool config();

     /**
         * Send command to wake sleppy client
         * Send Wake --> wait for Key --> Send Payload 
         * 
         * 
         * 
         * 
         */
     bool sleepyWake(uint16_t address, uint8_t channel, String payload);

     /**
         * sleppy client  recive command
         * After wake -->  Send key --> wait for payload
         * 
         * 
         * 
         * 
         */
     String sleepyIsWake();

     /**
         * Sensor client send data
         * Send Request --> wait for key --> send data
         * 
         * 
         * 
         * 
         */
     bool sensorSend(String payload);

     /**
         * Recieve request -> send key -> recieve data
         * 
         * 
         */
     String ServerRecieve();

     /**
         * 
         * 
         * 
         * 
         * 
         */
     void loop();


     /**
      * 
      * 
      * 
      */
     bool available();


private:
     uint8_t _haddress, _laddress, _channel;
     LoRa_E32 *_lora;
     bool _bootloader_random  , e32cp_stop_lissen = false;
     OnE32ReciveMessage e32_function_callback;

     uint8_t *OneTimePassword();
};

#endif