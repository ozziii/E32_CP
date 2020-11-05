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



#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <functional>


#include <LoRa_E32.h>

#include <AES.h>

#define E32_SERVER_ADDRESS 1
#define E32_SERVER_CHANNEL 1

#define E32_WAKE_COMMAND "wake"

#define E32_HANDUP_COMMAND "handup"

#define E32_WAKE_DELAY 3000

#define E32_KEY_LENGTH 16


static uint8_t E32_PSKEY[E32_KEY_LENGTH] = 
	{
		0x11 , 0x22, 0x33 , 0x44,
		0x55 , 0x66, 0x77 , 0x88,
		0x99 , 0xAA, 0xBB , 0xCC,
		0xDD , 0xEE, 0xA2 , 0xB3
	};

typedef std::function<void(String Payload)> OnE32ReciveMessage;

// COMMENT 
#define ASYNC_E32

#ifdef ASYNC_E32
#include "driver/uart.h"

static void IRAM_ATTR e32_uart_intr_handle(void *);
static uart_isr_handle_t * e32_handle_console;
#endif

class e32cp 
{
    public:
        /**
         * Client CTOR
         * 
         * 
         * 
         */ 
        e32cp(LoRa_E32 * lora,uint16_t address = E32_SERVER_ADDRESS,uint8_t channel = E32_SERVER_CHANNEL);


        /**
         *   MAKE ASYCRONUS URAT FUNCTION ONLY FOR HARDWARE SERIAL
         * 
         * 
         */
        void attachInterrupt(uart_port_t uart_number, OnE32ReciveMessage callback,e32cp * self);


        /**
         * 
         */
        void begin();
    
        /**
         * Send command to sleppy client
         * Send Wake --> wait for Key --> Send Payload 
         * 
         * 
         * 
         * 
         */
        bool sleepyWake(uint16_t address,uint8_t channel,String payload);


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
         * 
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

    //private:
        uint8_t _haddress,_laddress,_channel;
        LoRa_E32 * _lora;
        AES * _aes;

        uint8_t * OneTimePassword();
};


static e32cp * E32Self;
static OnE32ReciveMessage e32_function_callback;

static void IRAM_ATTR e32_uart_intr_handle(void *)
{
    String result =  E32Self->ServerRecieve();
    e32_function_callback(result);
}

#endif