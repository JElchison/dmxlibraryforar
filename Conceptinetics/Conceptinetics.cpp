/*
  Conceptinetics.cpp - DMX library for Arduino
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
*/


#include "pins_arduino.h"
#include "Conceptinetics.h"

#include <inttypes.h>
#include <stdlib.h>

#include <avr/interrupt.h>
#include <avr/io.h>

#include <util/delay.h>

#if defined (USART__TXC_vect)
  #define USART_TX USART__TXC_vect
#elif defined(USART_TX_vect)
  #define USART_TX  USART_TX_vect
#elif defined(USART0_TX_vect)
  #define USART_TX USART0_TX_vect
#endif 

#if defined (USART__RXC_vect)
  #define USART_RX USART__RXC_vect
#elif defined(USART_RX_vect)
  #define USART_RX  USART_RX_vect
#elif defined(USART0_RX_vect)
  #define USART_RX USART0_RX_vect
#endif 


#if defined UDR
  #define DMX_UDR UDR
#elif defined UDR0
  #define DMX_UDR UDR0
#endif


#if defined(UBRRH) && defined(UBRRL)
  #define DMX_UBRRH UBRRH
  #define DMX_UBRRL UBRRL
#elif defined(UBRR0H) && defined(UBRR0L)
  #define DMX_UBRRH UBRR0H
  #define DMX_UBRRL UBRR0L
#endif

#if defined(UCSRA)
  #define DMX_UCSRA UCSRA
#elif defined(UCSR0A)
  #define DMX_UCSRA UCSR0A
#endif

#if defined(FE)
  #define DMX_FE FE
#elif defined(FE0)
  #define DMX_FE FE0
#endif

#define LOWBYTE(v)   ((uint8_t) (v))
#define HIGHBYTE(v)  ((uint8_t) (((uint16_t) (v)) >> 8))

#define BSWAP_16(x)  ( (uint8_t)((x) >> 8) | ((uint8_t)(x)) << 8 )

namespace isr
{
    enum isrState
    {
        Idle,
        Break,
        DmxBreak,
        DmxBreakManual,
        DmxStartByte,   
        DmxRecordData,
        DmxTransmitData,
        RdmStartByte,
        RdmRecordData,
        RdmTransmitData,
    };

    enum isrMode
    {
        Disabled,
        Receive,
        DMXTransmit,
        DMXTransmitManual,  /* Manual break... */
        RDMTransmit,
        RDMTransmitNoInt,   /* Setup uart but leave interrupt disabled */
    };
};


DMX_Master      *__dmx_master;
DMX_Slave       *__dmx_slave;
RDM_Responder   *__rdm_responder;

int8_t          __re_pin;                               // R/W Pin on shield

isr::isrState   __isr_txState;                          // TX ISR state
isr::isrState   __isr_rxState;                          // RX ISR state


void SetISRMode ( isr::isrMode );


DMX_FrameBuffer::DMX_FrameBuffer ( uint16_t buffer_size )
{
    m_refcount = (uint8_t*) malloc ( sizeof ( uint8_t ) );

    if ( buffer_size >= DMX_MIN_FRAMESIZE && buffer_size <= DMX_MAX_FRAMESIZE )
    {
        m_buffer = (uint8_t*) malloc ( buffer_size );
        if ( m_buffer != NULL )
        {
            memset ( (void *)m_buffer, 0x0, buffer_size );
            m_bufferSize = buffer_size;
        }
        else 
            m_buffer = 0x0;
    }
    else
        m_bufferSize = 0x0;

    *m_refcount++;
}

DMX_FrameBuffer::DMX_FrameBuffer ( DMX_FrameBuffer &buffer )
{
    // Copy references and make sure the parent object does not dispose our
    // buffer when deleted and we are still active
    this->m_refcount = buffer.m_refcount;
    (*this->m_refcount)++;
    
    this->m_buffer = buffer.m_buffer;
    this->m_bufferSize = buffer.m_bufferSize;
}

DMX_FrameBuffer::~DMX_FrameBuffer ( void )
{
    // If we are the last object using the
    // allocated buffer then free it together
    // with the refcounter
    if ( --(*m_refcount) == 0 )
    {
        if ( m_buffer )
            free ( m_buffer );

        free ( m_refcount );
    }
}

uint16_t DMX_FrameBuffer::getBufferSize ( void )
{
    return m_bufferSize;
}        


uint8_t DMX_FrameBuffer::getSlotValue ( uint16_t index )
{
    if (index < m_bufferSize)
        return m_buffer[index];
    else
        return 0x0;
}


void DMX_FrameBuffer::setSlotValue ( uint16_t index, uint8_t value )
{
    if ( index < m_bufferSize )
        m_buffer[index] = value;
}


void DMX_FrameBuffer::setSlotRange ( uint16_t start, uint16_t end, uint8_t value )
{
    if ( start < m_bufferSize && end < m_bufferSize && start < end )
        memset ( (void *) &m_buffer[start], value, end-start );
}

void DMX_FrameBuffer::clear ( void )
{
    memset ( (void *) m_buffer, 0x0, m_bufferSize );
}        

uint8_t &DMX_FrameBuffer::operator[] ( uint16_t index )
{
    return m_buffer[index];
}


DMX_Master::DMX_Master ( DMX_FrameBuffer &buffer, int readEnablePin )
: m_frameBuffer ( buffer ), 
  m_autoBreak ( 1 )                                     // Autobreak generation is default on
{
    setStartCode ( DMX_START_CODE );    

    __re_pin = readEnablePin;
    ::SetISRMode ( isr::Disabled );
}

DMX_Master::DMX_Master ( uint16_t maxChannel, int readEnablePin )
: m_frameBuffer ( maxChannel + DMX_STARTCODE_SIZE ), 
  m_autoBreak ( 1 )                                     // Autobreak generation is default on
{
    setStartCode ( DMX_START_CODE );

    __re_pin = readEnablePin;
    ::SetISRMode ( isr::Disabled );
}

DMX_Master::~DMX_Master ( void )
{
    disable ();                                         // Stop sending
    __dmx_master = NULL;
}

DMX_FrameBuffer &DMX_Master::getBuffer ( void )
{
    return m_frameBuffer;                               // Return reference to frame buffer
}

void DMX_Master::setStartCode ( uint8_t value )
{
    m_frameBuffer[0] = value;                           // Set the first byte in our frame buffer
}

void DMX_Master::setChannelValue ( uint16_t channel, uint8_t value )
{
    if ( channel > 0 )                                  // Prevent overwriting the start code
        m_frameBuffer.setSlotValue ( channel, value );
}

void DMX_Master::setChannelRange ( uint16_t start, uint16_t end, uint8_t value )
{
    if ( start > 0 )                                    // Prevent overwriting the start code
        m_frameBuffer.setSlotRange ( start, end, value );
}


void DMX_Master::enable  ( void )
{
    __dmx_master = this;  

    if ( m_autoBreak )
        ::SetISRMode ( isr::DMXTransmit );
    else
        ::SetISRMode ( isr::DMXTransmitManual );
}

void DMX_Master::disable ( void )
{
     ::SetISRMode ( isr::Disabled );
    __dmx_master = NULL;                                // No active master
}

void    DMX_Master::setAutoBreakMode ( void ) { m_autoBreak = 1; }
void    DMX_Master::setManualBreakMode ( void ) { m_autoBreak = 0; }
uint8_t DMX_Master::autoBreakEnabled ( void ) { return m_autoBreak; }


uint8_t DMX_Master::waitingBreak ( void )
{
    return ( __isr_txState == isr::DmxBreakManual );
}
        
void DMX_Master::breakAndContinue ( uint8_t breakLength_us )
{
    // Only execute if we are the controlling master object
    if ( __dmx_master == this && __isr_txState == isr::DmxBreakManual )
    {
        pinMode ( 1, OUTPUT );
        digitalWrite ( 1, LOW );                    // Begin BREAK                               

        _delay_us ( breakLength_us );

        // Turn TX Pin into Logic HIGH
        digitalWrite ( 1, HIGH );                   // END BREAK

        __isr_txState = isr::DmxStartByte;
   
        // TX Enable
        #if defined(UCSRC) && defined(UCSRB)
	        UCSRB |= (1<<TXEN);								
        #elif defined(UCSR0C) && defined(UCSR0B)
	        UCSR0B |= (1<<TXEN0);
        #endif 

        _delay_us ( 12 );                           // MAB 12µSec
        
        // TX Interupt enable
        #if defined(UCSRC) && defined(UCSRB)
	        UCSRB |= (1<<TXCIE);								
        #elif defined(UCSR0C) && defined(UCSR0B)
	        UCSR0B |= (1<<TXCIE0);
        #endif 
    }
}




DMX_Slave::DMX_Slave ( DMX_FrameBuffer &buffer, int readEnablePin )
: DMX_FrameBuffer ( buffer ), 
  m_startAddress ( 1 )
{
    __dmx_slave = this;
    __re_pin    = readEnablePin;

    ::SetISRMode ( isr::Disabled );
}

DMX_Slave::DMX_Slave ( uint16_t nrChannels, int readEnablePin )
: DMX_FrameBuffer ( nrChannels + 1 ), 
  m_startAddress ( 1 )
{
    __dmx_slave = this;
    __re_pin    = readEnablePin;

    ::SetISRMode ( isr::Disabled );
}

DMX_Slave::~DMX_Slave ( void )
{
    disable ();
    __dmx_slave = NULL;
}


void DMX_Slave::enable ( void )
{
    ::SetISRMode ( isr::Receive );
}

void DMX_Slave::disable ( void )
{
    ::SetISRMode ( isr::Disabled );
}

DMX_FrameBuffer &DMX_Slave::getBuffer ( void )
{
    return reinterpret_cast<DMX_FrameBuffer&>(*this);
}

    uint8_t DMX_Slave::getChannelValue ( uint16_t channel )
{
    return getSlotValue ( channel );
}


uint16_t DMX_Slave::getStartAddress ( void )
{
    return m_startAddress;
}

void DMX_Slave::setStartAddress ( uint16_t addr )
{
    m_startAddress = addr;
}


bool DMX_Slave::processIncoming ( uint8_t val, bool first )
{
    static uint16_t idx;
    bool            rval = false;

    if ( first ) m_state = dmx::dmxStartByte;

    switch ( m_state )
    {
        case dmx::dmxStartByte:
            setSlotValue ( 0, val );    // Store start code
            idx = m_startAddress;
            m_state = dmx::dmxWaitStartAddress;

        case dmx::dmxWaitStartAddress:
            if ( --idx == 0 )
                m_state = dmx::dmxData;
            break;

        case dmx::dmxData:
            if ( idx++ < getBufferSize() )
                setSlotValue ( idx, val );
            else
            {
                m_state = dmx::dmxFrameReady;
                rval = true;
            }
            break;
    }

    return rval;
}


uint16_t RDM_FrameBuffer::getBufferSize ( void ) { return sizeof ( m_msg ); }   

uint8_t RDM_FrameBuffer::getSlotValue ( uint16_t index )
{
    if ( index < sizeof ( m_msg ) )
        return m_msg.d[index];
    else
        return 0x0;
}


void RDM_FrameBuffer::setSlotValue ( uint16_t index, uint8_t value )
{
    if ( index < sizeof ( m_msg ) )
        m_msg.d[index] = value;
}

void RDM_FrameBuffer::clear ( void )
{
    memset ( (void*)m_msg.d, 0x0, sizeof( m_msg ) ); 
    m_state             = rdm::rdmUnknown;
}

bool RDM_FrameBuffer::processIncoming ( uint8_t val, bool first )
{
    static uint16_t idx;
    bool            rval = false;

    if ( first )
    {
        m_state = rdm::rdmStartByte;
        m_csCalc.checksum   = (uint16_t) 0x0000;
        idx = 0;
    }

    // Prevent buffer overflow for large messages
    if (idx >= sizeof(m_msg))
        return true;

    switch ( m_state )
    {
        case rdm::rdmStartByte: 
            m_msg.startCode = val;
            m_state = rdm::rdmSubStartCode;
            break;

        case rdm::rdmSubStartCode:
            if ( val != 0x01 )
            {
                rval = true;                        // Stop processing data
                break;
            }

            m_msg.subStartCode = val;
            m_state = rdm::rdmMessageLength;
            break;

        case rdm::rdmMessageLength:
            m_msg.msgLength = val;
            m_state = rdm::rdmData;
            m_csCalc.checksum = 0xcc + 0x01 + val;  // set initial checksum 
            idx = 3;                                // buffer index for next byte
            break;

        case rdm::rdmData:
            m_msg.d[idx++] = val;
            m_csCalc.checksum += val;
            if ( idx >= m_msg.msgLength )
                m_state = rdm::rdmChecksumHigh;
            break;

        case rdm::rdmChecksumHigh:
            m_csRecv.csh = val;
            m_state = rdm::rdmChecksumLow;
            
            break;

        case rdm::rdmChecksumLow:
            m_csRecv.csl = val;

            if ((m_csCalc.checksum % (uint16_t)0x10000) == m_csRecv.checksum)
            { 
                m_state = rdm::rdmFrameReady;
                
                // valid checksum ... start processing
                processFrame ();
            }

            m_state = rdm::rdmUnknown;
            rval = true;
            break;
    };

    return rval;
}

bool RDM_FrameBuffer::fetchOutgoing ( volatile uint8_t *udr, bool first )
{
    static uint16_t idx;
    bool            rval = false;


    if ( first )
    {
        m_state             = rdm::rdmData;
        m_csCalc.checksum   = (uint16_t) 0x0000;
        idx                 = 0;
    }

    switch ( m_state )
    {
        case rdm::rdmData:
            m_csCalc.checksum += m_msg.d[idx];
            *udr = m_msg.d[idx++];
            if ( idx >= m_msg.msgLength )
            {
                m_csCalc.checksum = (m_csCalc.checksum % (uint16_t)0x10000);
                m_state = rdm::rdmChecksumHigh;
            }
            break;
        
        case rdm::rdmChecksumHigh:
            *udr = m_csCalc.csh;
            m_state = rdm::rdmChecksumLow;
            break;

        case rdm::rdmChecksumLow:
            *udr = m_csCalc.csl;
            m_state = rdm::rdmUnknown;
            rval = true;
            break;

    }

    return rval;
}


void (*RDM_Responder::event_onIdentifyDevice)(bool);

//
// slave parameter is only used to ensure a slave object is present before
// initializing the rdm responder class
//
RDM_Responder::RDM_Responder ( uint16_t m, uint8_t d1, uint8_t d2, 
                               uint8_t d3, uint8_t d4, DMX_Slave &slave )
:   RDM_FrameBuffer ( ),
    m_Personalities (1),    // Available personlities
    m_Personality (1)       // Default personality eq 1.
{
    __rdm_responder = this;
    m_devid.Initialize ( m, d1, d2, d3, d4 );
}

RDM_Responder::~RDM_Responder ( void )
{
    __rdm_responder = NULL;
}

void RDM_Responder::onIdentifyDevice ( void (*func)(bool) )
{
    event_onIdentifyDevice = func;
}

void RDM_Responder::repondDiscUniqueBranch ( void )
{
    #if defined(UCSRB)
	UCSRB  = (1<<TXEN);								
    #elif defined(UCSR0B)
	UCSR0B = (1<<TXEN0);
    #endif 

    uint16_t cs = 0;

    uint8_t response[24] =
    {
    0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xaa,     // byte 0-7
    m_devid.m_id[0] | 0xaa, m_devid.m_id[0] | 0x55,     // byte 8, 10   MSB manufacturer
    m_devid.m_id[1] | 0xaa, m_devid.m_id[1] | 0x55,     // byte 10, 11  LSB manufacturer
    m_devid.m_id[2] | 0xaa, m_devid.m_id[2] | 0x55,     // byte 12, 13  MSB device
    m_devid.m_id[3] | 0xaa, m_devid.m_id[3] | 0x55,     // byte 14, 15   .
    m_devid.m_id[4] | 0xaa, m_devid.m_id[4] | 0x55,     // byte 16, 17   .
    m_devid.m_id[5] | 0xaa, m_devid.m_id[5] | 0x55,     // byte 18, 19  LSB device
    0x0, 0x0, 0x0, 0x0                                  // Checksum space
    };

    // Calculate checksum
    for ( int i=8; i<20; i++ )
        cs += (uint16_t)response [i];

    // Write checksum into response
    response [20] = HIGHBYTE (cs) | 0xaa;
    response [21] = HIGHBYTE (cs) | 0x55;
    response [22] = LOWBYTE  (cs) | 0xaa;
    response [23] = LOWBYTE  (cs) | 0x55;

    // Table 3-2 ANSI_E1-20-2010 
    _delay_us ( MIN_RESPONDER_PACKET_SPACING_USEC );

    // Set shield to transmit mode (turn arround)
    digitalWrite ( __re_pin, HIGH );

    for ( int i=0; i<24; i++ )
    {
        // Wait until data register is empty
        #if defined (UCSR0A) && defined (UDRE0)
        while((UCSR0A & (1 <<UDRE0)) == 0);	
        #elif defined (UCSRA) && defined (UDRE)
        while((UCSRA & (1 <<UDRE)) == 0);	
        #endif
        
        DMX_UDR = response[i];
    }

    // Wait until last byte is send
    #if defined (UCSR0A) && defined (UDRE0)
    while((UCSR0A & (1 <<UDRE0)) == 0);	
    #elif defined (UCSRA) && defined (UDRE)
    while((UCSRA & (1 <<UDRE)) == 0);	
    #endif

    // Restore ISR operations
    ::SetISRMode ( isr::Receive );
}

void RDM_Responder::populateDeviceInfo ( void )
{
    RDM__DeviceInfoPD *pd = reinterpret_cast<RDM__DeviceInfoPD *>(m_msg.PD);

    pd->protocolVersionMajor        = 0x01;
    pd->protocolVersionMinor        = 0x00;
    pd->deviceModelId               = BSWAP_16(m_DeviceModelId);
    pd->ProductCategory             = BSWAP_16(m_ProductCategory);
    pd->SoftwareVersionIdH          = BSWAP_16(m_SoftwareVersionId);
    pd->SoftwareVersionIdL          = 0;
    pd->DMX512FootPrint             = BSWAP_16(__dmx_slave->getBufferSize()-1); // eq buffersize-startbyte
    pd->DMX512CurrentPersonality    = m_Personality;
    pd->DMX512NumberPersonalities   = m_Personalities;
    pd->DMX512StartAddress          = BSWAP_16(__dmx_slave->getStartAddress());
    pd->SubDeviceCount              = 0x0; // Sub devices are not supported by this library
    pd->SensorCount                 = 0x0; // Sensors are not yet supported

    m_msg.PDL = sizeof (RDM__DeviceInfoPD);
}

const uint8_t ManufacturerLabel_P[] PROGMEM = "Conceptinetics"; 

void RDM_Responder::processFrame ( void )
{
    // If packet is a general broadcast   
    if (
        m_msg.dstUid.isBroadcast (m_devid.m_id) ||  
        m_devid == m_msg.dstUid
       )
    {
        // Set default response type
        m_msg.portId    = rdm::ResponseTypeAck; 
        
        switch ( BSWAP_16(m_msg.PID) )
        {
            case rdm::DiscUniqueBranch:
                // Check if we are inside the given unique branch...
                if ( !m_rdmStatus.mute &&
                     reinterpret_cast<RDM_DiscUniqueBranchPD *>(m_msg.PD)->lbound < m_devid &&
                     reinterpret_cast<RDM_DiscUniqueBranchPD *>(m_msg.PD)->hbound > m_devid )
                {
                    // Discovery messages are responded with data only and no breaks
                    repondDiscUniqueBranch ();
                }
                break;

            case rdm::DiscMute:
                digitalWrite (8, HIGH);
                reinterpret_cast<RDM_DiscMuteUnMutePD *>(m_msg.PD)->ctrlField = 0x0;
                m_msg.PDL = sizeof ( RDM_DiscMuteUnMutePD );
                m_rdmStatus.mute = true;
                digitalWrite (8, LOW);
                break;

            case rdm::DiscUnMute:
                reinterpret_cast<RDM_DiscMuteUnMutePD *>(m_msg.PD)->ctrlField = 0x0;
                m_msg.PDL = sizeof ( RDM_DiscMuteUnMutePD );
                m_rdmStatus.mute = false;
                break;

            case rdm::SupportedParameters:
                //
                // Temporary solution... this will become dynamic
                // in a later version...
                //
                m_msg.PD[0] = HIGHBYTE(rdm::DmxStartAddress);   // MSB
                m_msg.PD[1] = LOWBYTE (rdm::DmxStartAddress);   // LSB
                m_msg.PD[2] = HIGHBYTE(rdm::DmxPersonality);
                m_msg.PD[3] = LOWBYTE (rdm::DmxPersonality);
                m_msg.PD[4] = HIGHBYTE(rdm::ManufacturerLabel);
                m_msg.PD[5] = LOWBYTE (rdm::ManufacturerLabel);
                m_msg.PDL   = 0x6;
                break;

            // Only for manufacturer specific parameters
            // case rdm::ParameterDescription:
            //    break;

            case rdm::DeviceInfo:
                if ( m_msg.CC == rdm::GetCommand )
                    populateDeviceInfo ();
                break;

            case rdm::DmxStartAddress:                
                if ( m_msg.CC == rdm::GetCommand )
                {
                    m_msg.PD[0] = HIGHBYTE(__dmx_slave->getStartAddress ());
                    m_msg.PD[1] = LOWBYTE (__dmx_slave->getStartAddress ());
                    m_msg.PDL   = 0x2;
                }
                else // if (  m_msg.CC == rdm::SetCommand  )
                {
                    __dmx_slave->setStartAddress ( (m_msg.PD[0] << 8) + m_msg.PD[1] );
                    m_msg.PDL   = 0x0;
                }
                break;

            case rdm::DmxPersonality:
                if ( m_msg.CC == rdm::GetCommand )
                {
                    reinterpret_cast<RDM_DeviceGetPersonality_PD *>
                        (m_msg.PD)->DMX512CurrentPersonality = m_Personality;
                    reinterpret_cast<RDM_DeviceGetPersonality_PD *>
                        (m_msg.PD)->DMX512CurrentPersonality = m_Personalities;
                    m_msg.PDL   = sizeof (RDM_DeviceGetPersonality_PD);
                }
                else // if (  m_msg.CC == rdm::SetCommand  )
                {
                     m_Personality = reinterpret_cast<RDM_DeviceSetPersonality_PD *>
                        (m_msg.PD)->DMX512Personality;
                     m_msg.PDL = 0x0;
                } 
                break;

            case rdm::IdentifyDevice:
                if ( m_msg.CC == rdm::GetCommand )
                {
                    m_msg.PD[0] = (uint8_t)(m_rdmStatus.ident ? 1 : 0);
                    m_msg.PDL   = 0x1;
                }
                else if (  m_msg.CC == rdm::SetCommand  )
                {
                    // Look into first byte to see whether identification
                    // is turned on or off 
                    m_rdmStatus.ident = m_msg.PD[0] ? true : false;
                    if ( event_onIdentifyDevice )
                        event_onIdentifyDevice ( m_rdmStatus.ident );

                     m_msg.PDL   = 0x0;
                }
                break;

            case rdm::ManufacturerLabel:
                if ( m_msg.CC == rdm::GetCommand )
                {
                    memcpy_P( (void*)m_msg.PD, ManufacturerLabel_P, sizeof(ManufacturerLabel_P) );
    				m_msg.PDL = sizeof ( ManufacturerLabel_P );
                }

            default:
                // Unknown parameter ID response
                m_msg.portId    = rdm::ResponseTypeNackReason;
                m_msg.PD[0]     = rdm::UnknownPid;
                m_msg.PD[1]     = 0x0;
                m_msg.PDL       = 0x2;
                break;
        };
    }

    //
    // Only respond if this this message
    // was destined to us only
    if ( m_msg.dstUid == m_devid )
    {
        m_msg.startCode     = RDM_START_CODE;
        m_msg.subStartCode  = 0x01;
        m_msg.msgLength     = RDM_HDR_LEN + m_msg.PDL;
        m_msg.msgCount      = 0;

        /*
        switch ( m_msg.msg.CC )
        {
            case rdm::DiscoveryCommand:
                m_msg.msg.CC = rdm::DiscoveryCommandResponse;
                break;
            case rdm::GetCommand:
                m_msg.msg.CC = rdm::GetCommandResponse;
                break;
            case rdm::SetCommand:
                m_msg.msg.CC = rdm::SetCommandResponse;
                break;
        }
        */ 
        /* Above replaced by next line */
        m_msg.CC++;

        m_msg.dstUid.copy ( m_msg.srcUid );
        m_msg.srcUid.copy ( m_devid );

        _delay_us ( MIN_RESPONDER_PACKET_SPACING_USEC );

        SetISRMode ( isr::RDMTransmit );
    }
}


void SetISRMode ( isr::isrMode mode )
{
    uint8_t readEnable;

#if defined(UCSRB) && defined (UCSRC)
    UCSRC |= (1<<UMSEL)|(3<<UCSZ0)|(1<<USBS);
#elif defined(UCSR0B) && defined (UCSR0C)
    UCSR0C |= (3<<UCSZ00)|(1<<USBS0);  
#endif                

    switch ( mode )
    {
        case isr::Disabled:
            #if defined(UCSRB)
                UCSRB  = 0x0;
            #elif defined(UCSR0B)
        	    UCSR0B = 0x0;
            #endif    
            readEnable = LOW;
            break;

        case isr::Receive:
            DMX_UBRRH = (unsigned char)(((F_CPU + DMX_BAUD_RATE * 8L) / (DMX_BAUD_RATE * 16L) - 1)>>8);
    	    DMX_UBRRL = (unsigned char) ((F_CPU + DMX_BAUD_RATE * 8L) / (DMX_BAUD_RATE * 16L) - 1);	

            // Prepare before kicking off ISR
	        //DMX_UDR             = 0x0;
	        __isr_rxState       = isr::Idle;        
            readEnable          = LOW; 
            #if defined(UCSRB)
        	    UCSRB  = (1<<RXCIE)|(1<<RXEN);	
            #elif defined(UCSR0B)
        	    UCSR0B = (1<<RXCIE0)|(1<<RXEN0);        // Enable receive
            #endif                
            break;

        case isr::DMXTransmit:
            DMX_UDR         = 0x0;                              
            readEnable      = HIGH;
            __isr_txState   = isr::DmxBreak; 
            #if defined(UCSRB)
	            UCSRB  = (1<<TXEN) |(1<<TXCIE);								
            #elif defined(UCSR0B)
	            UCSR0B = (1<<TXEN0) |(1<<TXCIE0);
            #endif 
            break;

        case isr::DMXTransmitManual:
            DMX_UBRRH = (unsigned char)(((F_CPU + DMX_BAUD_RATE * 8L) / (DMX_BAUD_RATE * 16L) - 1)>>8);
    	    DMX_UBRRL = (unsigned char) ((F_CPU + DMX_BAUD_RATE * 8L) / (DMX_BAUD_RATE * 16L) - 1);	
            DMX_UDR = 0x0;
            #if defined(UCSRB)
                UCSRB  = 0x0; 
            #elif defined(UCSR0B)
                UCSR0B = 0x0; 
            #endif    
            readEnable      = HIGH;
             __isr_txState  = isr::DmxBreakManual;
            break;

        case isr::RDMTransmit:
            // If read enable pin is assigned
            DMX_UBRRH = (unsigned char)(((F_CPU + DMX_BREAK_RATE * 8L) / (DMX_BREAK_RATE * 16L) - 1)>>8);
            DMX_UBRRL = (unsigned char) ((F_CPU + DMX_BREAK_RATE * 8L) / (DMX_BREAK_RATE * 16L) - 1);
            readEnable      = HIGH;
            __isr_txState   = isr::RdmStartByte; 
            #if defined(UCSRB)
	            UCSRB  = (1<<TXEN) |(1<<TXCIE);								
            #elif defined(UCSR0B)
	            UCSR0B = (1<<TXEN0) |(1<<TXCIE0);
            #endif 
            DMX_UDR=0;
            break;
    }

    // If read enable pin is assigned
    if (__re_pin > -1)
        digitalWrite ( __re_pin, readEnable );

}

//
// TX UART (DMX Transmission ISR)
//
ISR (USART_TX)
{
	static uint16_t			current_slot;

	switch ( __isr_txState )
	{
	case isr::DmxBreak:
		DMX_UBRRH = (unsigned char)(((F_CPU + DMX_BREAK_RATE * 8L) / (DMX_BREAK_RATE * 16L) - 1)>>8);
        DMX_UBRRL = (unsigned char) ((F_CPU + DMX_BREAK_RATE * 8L) / (DMX_BREAK_RATE * 16L) - 1);
        DMX_UDR = 0x0;
        
        if ( __isr_txState ==  isr::DmxBreak )
            __isr_txState = isr::DmxStartByte;
        
        break;

	case isr::DmxStartByte:
		DMX_UBRRH = (unsigned char)(((F_CPU + DMX_BAUD_RATE * 8L) / (DMX_BAUD_RATE * 16L) - 1)>>8);
		DMX_UBRRL = (unsigned char) ((F_CPU + DMX_BAUD_RATE * 8L) / (DMX_BAUD_RATE * 16L) - 1);						
        current_slot = 0;	
        DMX_UDR = __dmx_master->getBuffer()[ current_slot++ ];
		__isr_txState = isr::DmxTransmitData;
		break;
	

	case isr::DmxTransmitData:
        // NOTE: we always send full frames of 513 bytes, this will bring us 
        // close to 40 frames / sec with no interslot delays
        #ifdef DMX_IBG
            _delay_us (DMX_IBG);
        #endif

		DMX_UDR = __dmx_master->getBuffer().getSlotValue( current_slot++ );
			
		// Send 512 channels
		if ( current_slot >= DMX_MAX_FRAMESIZE )
        {
		    if ( __dmx_master->autoBreakEnabled () )
                __isr_txState = isr::DmxBreak;
            else
                SetISRMode ( isr::DMXTransmitManual );
	    }
        
		break;

    case isr::RdmStartByte:
        DMX_UBRRH = (unsigned char)(((F_CPU + DMX_BAUD_RATE * 8L) / (DMX_BAUD_RATE * 16L) - 1)>>8);
		DMX_UBRRL = (unsigned char) ((F_CPU + DMX_BAUD_RATE * 8L) / (DMX_BAUD_RATE * 16L) - 1);			

        // Write start byte
        __rdm_responder->fetchOutgoing ( &DMX_UDR, true );
        __isr_txState = isr::RdmTransmitData;

        break;

    case isr::RdmTransmitData:
        // Write rest of data
        if ( __rdm_responder->fetchOutgoing ( &DMX_UDR ) )
        {
            SetISRMode ( isr::Receive );    // Start waitin for new data
            __isr_txState = isr::Idle;      // No tx state
        }
        break;
    }
}



//
// RX UART (DMX Reception ISR)
//
ISR (USART_RX)
{
    uint8_t usart_state    = DMX_UCSRA;
    uint8_t usart_data     = DMX_UDR;

    //
    // Check for framing error and reset if found
    // A framing most likely* indicate a break in our ocasion
    //
    if ( usart_state & (1<<DMX_FE) )
	{
	    DMX_UCSRA &= ~(1<<DMX_FE);
        __isr_rxState = isr::Break;
        return;
    }
    
    switch ( __isr_rxState )
    {
        case isr::Break:
            if ( __dmx_slave && usart_data == DMX_START_CODE )
            {
                __dmx_slave->processIncoming ( usart_data, true );
                __isr_rxState = isr::DmxRecordData;
            }
            else if ( __rdm_responder && usart_data == RDM_START_CODE )
            {
                // __rdm_responder->clear ();
                __rdm_responder->processIncoming ( usart_data, true );
                __isr_rxState = isr::RdmRecordData;
            }
            else
            {
                __isr_rxState = isr::Idle;
            }
            break;

        // Process DMX Data
        case isr::DmxRecordData:
            if ( __dmx_slave->processIncoming ( usart_data ) )
               __isr_rxState = isr::Idle;
            break;

        // Process RDM Data
        case isr::RdmRecordData:
            if ( __rdm_responder->processIncoming ( usart_data ) )
                __isr_rxState = isr::Idle;
            break;

    }
}

