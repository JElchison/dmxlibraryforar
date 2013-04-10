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

    //
    // Bytes already swapped here to meet endianness 
    //
    enum RdmParameters
    {
        // Category - Network Management
        DiscUniqueBranch                = 0x0100,   // Required
        DiscMute                        = 0x0200,   // Required
        DiscUnMute                      = 0x0300,   // Required

        CommsStatus                     = 0x1500,   // Get,Set

        // Category - Status Collection
        QueuedMessage                   = 0x2000,   // Get      [enum RdmStatusTypes]
        StatusMessages                  = 0x3000,   // Get      [enum RdmStatusTypes]
        StatusIdDescription             = 0x3100,   // Get
        ClearStatusId                   = 0x3200,   // Set
        SubDeviceStatusReportThreshold  = 0x3300,   // Get, Set [enum RdmStatusTypes]

        // Category - RDM Information
        // ** Only required 9f supporting parameters 
        //    beyond the minimum required set
        SupportedParameters             = 0x5000,   // Get, **Required
        ParameterDescription            = 0x5100,   // Get, **Required
    
        // Category = Product Information
        DeviceInfo                      = 0x6000,   // Get, Required
        ProductDetailIdList             = 0x7000,   // Get
        DeviceModelDescription          = 0x8000,   // Get
        ManufacturerLabel               = 0x8100,   // Get
        DeviceLabel                     = 0x8200,   // Get, Set
        FactoryDefaults                 = 0x9000,   // Get, Set **
        SoftwareVersionLabel            = 0xc000,   // Get
      
        // Category - DMX512 Setup
        DmxPersonality                  = 0xe000,   // Get, Set
        DmxPersonalityDescription       = 0xe100,   // Get
        DmxStartAddress                 = 0xf000,   // Get, Set ** Required if DMX device
        SlotInfo                        = 0x2001,   // Get
        SlotDescription                 = 0x2101,   // Get
        DefaultSlotValue                = 0x2201,   // Get

        // Category - Sensors
        // Category - Dimmer Settings
        // Category - Power/Lamp Settings
        // Category - Display Settings
        // Category - Configuration

        // Category - Control
        IdentifyDevice                  = 0x0010,   // Get, Set, Required
        ResetDevice                     = 0x0110,   // Set
        PowerState                      = 0x1010,   // Get, Set
        PerformSelftest                 = 0x2010,   // Get, Set
        SelfTestDescription             = 0x2110,   // Get
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
        ProductCategoryNotDeclared      = 0x0000,
    };

    enum RdmProductDetail
    {
        ProductDetailNotDeclared        = 0x0000,
    };


};


#endif /* RDM_DEFINES_H_ */
