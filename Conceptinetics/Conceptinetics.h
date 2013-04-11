/*
  Conceptinetics.h - DMX library for Arduino
  Copyright (c) 2013 W.A. van der Meeren <danny@illogic.nl>.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 3 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
  This code has been tested using the following hardware:

  - Arduino UNO R3 using a CTC-DRA-13-1 ISOLATED DMX-RDM SHIELD 
  - Arduino MEGA2560 R3 using a CTC-DRA-13-1 ISOLATED DMX-RDM SHIELD 

  - CTC-DRA-12-1 is the Non-isolated costs effective DMX-RDM shield 
*/


#ifndef CONCEPTINETICS_H_
#define CONCEPTINETICS_H_

#include <Arduino.h>
#include <inttypes.h>

#include "Rdm_Uid.h"
#include "Rdm_Defines.h"

#define DMX_MAX_FRAMESIZE       513     // Startbyte + 512 Slots
#define DMX_MIN_FRAMESIZE       2       // Startbyte + 1 Slot

#define DMX_MAX_FRAMECHANNELS   512     // Maxmim number of channer per frame

#define DMX_STARTCODE_SIZE      1       // Size of startcode in bytes

#define DMX_START_CODE          0x0     // Start code for a DMX frame
#define RDM_START_CODE          0xcc    // Start code for a RDM frame

// Uncomment to enable interbyte gaps (Interslot times ) (avg < 76uSec) ... 
// mimum is zero according to specification
// #define DMX_IBG				    10      // Interbyte gap

// Speed your Arduino is running on in Hz.
#define F_OSC 				    16000000UL

// DMX Baudrate, this should be 25000
#define DMX_BAUD_RATE 		    250000

// The baudrate used to automaticly generate a break within
// your ISR.. make it lower to generate longer breaks
#define DMX_BREAK_RATE 	 	    99900       

#define RDM_HDR_LEN             24      // RDM Message header length ** fixed
#define RDM_PD_MAXLEN           32      // RDM Maximum parameter data length 1 - 231


namespace dmx 
{
    enum dmxState 
	{
        dmxUnknown,
        dmxStartByte,
        dmxWaitStartAddress,
        dmxData,
        dmxFrameReady,
	};
};

namespace rdm
{
    enum rdmState
    {
        rdmUnknown,
        rdmStartByte,
        rdmSubStartCode,
        rdmMessageLength,
        rdmData,
        rdmChecksumHigh,
        rdmChecksumLow,
        rdmFrameReady,
    };
}

struct IFrameBuffer
{
    virtual uint16_t    getBufferSize   ( void ) = 0;        

    virtual uint8_t     getSlotValue    ( uint16_t index ) = 0;
    virtual void        setSlotValue    ( uint16_t index, uint8_t value ) = 0;
};

class DMX_FrameBuffer : IFrameBuffer
{
    public:
        //
        // Constructor buffersize = 1-513
        //
        DMX_FrameBuffer     ( uint16_t buffer_size );
        DMX_FrameBuffer     ( DMX_FrameBuffer &buffer );
        ~DMX_FrameBuffer    ( void );

        uint16_t getBufferSize ( void );        

        uint8_t getSlotValue ( uint16_t index );
        void    setSlotValue ( uint16_t index, uint8_t value );
        void    setSlotRange ( uint16_t start, uint16_t end, uint8_t value );
        void    clear ( void );        

        uint8_t &operator[] ( uint16_t index );

    private:

        uint8_t     *m_refcount;
        uint16_t    m_bufferSize;
        uint8_t     *m_buffer;      
};


//
// DMX Master controller
//
class DMX_Master
{
    public:
        // Run the DMX master from a pre allocated frame buffer which
        // you have fully under your own control
        DMX_Master ( DMX_FrameBuffer &buffer, int readEnablePin  );
        
        // Run the DMX master by giving a predefined maximum number of
        // channels to support
        DMX_Master ( uint16_t maxChannel, int readEnablePin );

        ~DMX_Master ( void );
    
        void enable  ( void );              // Start transmitting
        void disable ( void );              // Stop transmitting

        // Get reference to the internal framebuffer
        DMX_FrameBuffer &getBuffer ( void );

        // Update channel values
        void setChannelValue ( uint16_t channel, uint8_t value );
        void setChannelRange ( uint16_t start, uint16_t end, uint8_t value );

    public:
        //
        // Manual control over the break period
        //
        void setAutoBreakMode ( void );     // Generated from ISR
        void setManualBreakMode ( void );   // Generate manually

        uint8_t autoBreakEnabled ( void );

        // We are waiting for a manual break to be generated 
        uint8_t waitingBreak ( void );
        
        // Generate break and start transmission of frame
        void breakAndContinue ( uint8_t breakLength_us = 100 );


    protected:
        void setStartCode ( uint8_t value ); 


    private:
        DMX_FrameBuffer m_frameBuffer;
        uint8_t         m_autoBreak;
};


//
// DMX Slave controller
//
class DMX_Slave : DMX_FrameBuffer
{
    public:
        DMX_Slave ( DMX_FrameBuffer &buffer, int readEnablePin = -1 );

        DMX_Slave ( uint16_t nrChannels, int readEnablePin = -1 );

        ~DMX_Slave ( void );

        void enable     ( void );           // Enable receiver
        void disable    ( void );           // Disable receiver

 
        // Get reference to the internal framebuffer
        DMX_FrameBuffer &getBuffer ( void );

        uint8_t  getChannelValue ( uint16_t channel );

        uint16_t getStartAddress ( void );
        void     setStartAddress ( uint16_t );


        // Process incoming byte from USART
        bool processIncoming   ( uint8_t val, bool first = false );

    protected:


    private:
        uint16_t        m_startAddress;     // Slave start address
        dmx::dmxState   m_state;
};


union RDM_Message
{
    uint8_t         d[ RDM_HDR_LEN + RDM_PD_MAXLEN ];
    struct
    {
        uint8_t     startCode;        // 0        SC_RDM
        uint8_t     subStartCode;     // 1        SC_SUB_MESSAGE
        uint8_t     msgLength;        // 2        Range 24 - 255
        RDM_Uid     dstUid();         // 3-8      Destination UID
        RDM_Uid     srcUid();         // 9-14     Source UID (sender)
        uint8_t     TN;               // 15       Transaction number
        uint8_t     portId;           // 16       Port ID / Response type
        uint8_t     msgCount;         // 17
        uint16_t    subDevice;        // 18,19    0=root, 0xffff=all
        uint8_t     CC;               // 20       GET_COMMAND
        uint16_t    PID;              // 21,22    Parameter ID
        uint8_t     PDL;              // 23       Parameter Data length 1-231 

        uint8_t     PD[RDM_PD_MAXLEN];    // Parameter Data ... variable length 
    } msg;
};

union RDM_Checksum
{
    uint16_t checksum;
    struct
    {
        uint8_t csl;
        uint8_t csh;
    };
};

class RDM_FrameBuffer : IFrameBuffer
{
    public:
        //
        // Constructor
        //
        RDM_FrameBuffer     ( void ) {};
        ~RDM_FrameBuffer    ( void ) {};

        uint16_t getBufferSize ( void );        

        uint8_t getSlotValue ( uint16_t index );
        void    setSlotValue ( uint16_t index, uint8_t value );
        void    clear ( void );        

        uint8_t &operator[] ( uint16_t index );

       
        // Process incoming byte from USART
        bool processIncoming ( uint8_t val, bool first = false );
        
    protected:
        bool processFrame ( void );

    private:
        rdm::rdmState   m_state;       // State for pushing the message in
        RDM_Message     m_msg;
        RDM_Checksum    m_csRecv;      // Checksum received in rdm message
        RDM_Checksum    m_csCalc;      // Calculared checksum
};

//
// RDM_Responder 
//
class RDM_Responder : public RDM_FrameBuffer
{
    public:
        //
        // m        = manufacturer id (16bits)
        // d1-d4    = device id (32bits)
        //
        RDM_Responder   ( uint16_t m, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4 );
        ~RDM_Responder  ( void );


    private:
        RDM_FrameBuffer m_buffer;   // Buffer to store RDM Data
        RDM_Uid         m_devid;    // Holds our unique device ID

};


#endif /* CONCEPTINETICS_H_ */
