/*
  Rdm_Defines.h - DMX library for Arduino with RDM (Remote Device Management) support
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


#ifndef RDM_DEFINES_H_
#define RDM_DEFINES_H_

#include "Rdm_Uid.h"

namespace rdm
{
    enum RdmCommandClass
    {
        DiscoveryCommand            = 0x10,
        DiscoveryCommandResponse,
        GetCommand                  = 0x20,
        GetCommandResponse,
        SetCommand                  = 0x30,
        SetCommandResponse,
    };

    enum RdmResponseTypes
    {
        ResponseTypeAck              = 0x00,
        ResponseTypeAckTimer,
        ResponseTypeNackReason,
        ResponseTypeAckOverflow,             // Additional response data available (see spec) 
    };

    enum RdmParameters
    {
        // Category - Network Management
        DiscUniqueBranch                = 0x0001,   // Required
        DiscMute                        = 0x0002,   // Required
        DiscUnMute                      = 0x0003,   // Required

        CommsStatus                     = 0x0015,   // Get,Set

        // Category - Status Collection
        QueuedMessage                   = 0x0020,   // Get      [enum RdmStatusTypes]
        StatusMessages                  = 0x0030,   // Get      [enum RdmStatusTypes]
        StatusIdDescription             = 0x0031,   // Get
        ClearStatusId                   = 0x0032,   // Set
        SubDeviceStatusReportThreshold  = 0x0033,   // Get, Set [enum RdmStatusTypes]

        // Category - RDM Information
        // ** Only required if supporting parameters 
        //    beyond the minimum required set
        SupportedParameters             = 0x0005,   // Get, **Required
        ParameterDescription            = 0x0051,   // Get, **Required
    
        // Category = Product Information
        DeviceInfo                      = 0x0060,   // Get, Required
        ProductDetailIdList             = 0x0070,   // Get
        DeviceModelDescription          = 0x0080,   // Get
        ManufacturerLabel               = 0x0081,   // Get
        DeviceLabel                     = 0x0082,   // Get, Set
        FactoryDefaults                 = 0x0009,   // Get, Set **
        SoftwareVersionLabel            = 0x000c,   // Get
      
        // Category - DMX512 Setup
        DmxPersonality                  = 0x000e,   // Get, Set
        DmxPersonalityDescription       = 0x00e1,   // Get
        DmxStartAddress                 = 0x000f,   // Get, Set ** Required if DMX device
        SlotInfo                        = 0x0120,   // Get
        SlotDescription                 = 0x0121,   // Get
        DefaultSlotValue                = 0x0122,   // Get

        // Category - Sensors
        // Category - Dimmer Settings
        // Category - Power/Lamp Settings
        // Category - Display Settings
        // Category - Configuration

        // Category - Control
        IdentifyDevice                  = 0x1000,   // Get, Set, Required
        ResetDevice                     = 0x1001,   // Set
        PowerState                      = 0x1010,   // Get, Set
        PerformSelftest                 = 0x1020,   // Get, Set
        SelfTestDescription             = 0x1021,   // Get
    };


    enum RdmStatusTypes
    {
        StatusNone              = 0x00,
        StatusGetLastMessage,
        StatusAdvisory,
        StatusWarning,
        StatusError,
        StatusAdvisoryCleared   = 0x12,
        StatusWarningCleared,
        StatusErrorCleared,
    };
   
    enum RdmProductCategory
    {
        ProductCategoryNotDeclared          = 0x0000,
        ProductCategoryFixture              = 0x0100,
        ProductCategoryFixtureFixed         = 0x0101,
        ProductCategoryFixtureMovingYoke    = 0x0102,
        ProductCategoryFixtureMovingMirror  = 0x0103,
        ProductCategoryFixtureOther         = 0x01ff,
        // TODO...
    };

    enum RdmProductDetail
    {
        ProductDetailNotDeclared        = 0x0000,
    };

    // Only LSB
    enum RdmNackReasons
    {
        UnknownPid                      = 0x00,
        FormatError,
        HardwareFault,
        ProxyReject,
        WriteProtect,
        UnsupportedCmdClass,
        DataOutOfRange,
        BufferFull,
        PacketSizeUnsupported,
        SubDeviceOutOfRange,
        ProxyBufferFull
    };

};


#define RDM_HDR_LEN             24      // RDM Message header length ** fixed
#define RDM_PD_MAXLEN           32      // RDM Maximum parameter data length 1 - 231


union RDM_Message
{
    uint8_t         d[ RDM_HDR_LEN + RDM_PD_MAXLEN ];
    struct
    {
        uint8_t     startCode;        // 0        SC_RDM
        uint8_t     subStartCode;     // 1        SC_SUB_MESSAGE
        uint8_t     msgLength;        // 2        Range 24 - 255
        RDM_Uid     dstUid;         // 3-8      Destination UID
        RDM_Uid     srcUid;         // 9-14     Source UID (sender)
        uint8_t     TN;               // 15       Transaction number
        uint8_t     portId;           // 16       Port ID / Response type
        uint8_t     msgCount;         // 17
        uint16_t    subDevice;        // 18,19    0=root, 0xffff=all
        uint8_t     CC;               // 20       GET_COMMAND
        uint16_t    PID;              // 21,22    Parameter ID
        uint8_t     PDL;              // 23       Parameter Data length 1-231 

        uint8_t     PD[RDM_PD_MAXLEN];    // Parameter Data ... variable length 
    };
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

struct RDM_DiscUniqueBranchPD
{
    RDM_Uid lbound;
    RDM_Uid hbound;
};

struct RDM_DiscMuteUnMutePD
{
    uint16_t    ctrlField;

// Only for multiple ports
//    RDM_Uid     bindingUid;
};

struct RDM__DeviceInfoPD
{
    uint8_t     protocolVersionMajor;
    uint8_t     protocolVersionMinor;
    uint16_t    deviceModelId;
    uint16_t    ProductCategory;    // enum RdmProductCategory
    uint32_t    SoftwareVersionId;
    uint16_t    DMX512FootPrint;
    uint8_t     DMX512CurrentPersonality;
    uint8_t     DMX512NumberPersonalities;
    uint16_t    DMX512StartAddress;
    uint16_t    SubDeviceCount;
    uint8_t     SensorCount;
};


#endif /* RDM_DEFINES_H_ */
