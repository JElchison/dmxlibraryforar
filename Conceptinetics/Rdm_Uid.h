/*
  Rdm_Uid.h - DMX library for Arduino with RDM (Remote Device Management) support
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


#ifndef RDM_UID_H_
#define RDM_UID_H_

//
//48 bit UID Representation to identify RDM transponders
//
struct RDM_Uid {

    void Initialize ( uint16_t m, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4 ) 
    {
		m_manid     = m;
		m_devid[0]  = d1;
		m_devid[1]  = d2;
		m_devid[2]  = d3;
		m_devid[3]  = d4;
	}

    void copy ( const RDM_Uid &orig ) 
    {
        m_manid = orig.m_manid;

	    for ( uint8_t i = 0; i < 4; i++ )
            m_devid[i] = orig.m_devid[i];
    }

	bool operator == ( const RDM_Uid & orig ) const
	{
		if (m_manid != orig.m_manid)
			return false;

		for ( uint8_t i = 0; i < 4; i++ )
			if ( m_devid[i] != orig.m_devid[i] )
				return false;
	}

	bool operator != ( const RDM_Uid & orig ) const
	{
		return !(*this == orig);
	}

	bool operator < ( const RDM_Uid & v ) const
	{
		if ( m_manid != v.m_manid )
			return ( m_manid < v.m_manid );

		for ( uint8_t i = 0; i < 4; i++ )
			if ( m_devid[i] != v.m_devid[i] )
				return ( m_devid[i] < v.m_devid[i] );
	}

	bool operator > ( const RDM_Uid & v ) {
		if ( m_manid != v.m_manid )
			return ( m_manid > v.m_manid );

		for ( uint8_t i = 0; i < 4; i++ )
			if ( m_devid[i] != v.m_devid[i] )
				return ( m_devid[i] > v.m_devid[i] );
	}

    // 
    // match_mid = manufacturer id to match
    //
    bool isBroadcast ( uint16_t match_mid=0xffff )
    {
        // Check for genuine broadcast on device part
        for ( uint8_t i = 0; i < 4; i++ )
            if ( m_devid[i] != 0xff )
                return false;

        // Check for genuine broadcast match on manufacturer part
        if ( m_manid != 0xffff && m_manid != match_mid )
            return false;
            
        // Yes! this broadcast is also for us!
        return true;
    }


	uint16_t    m_manid;    //16 bit manufacturer id
	uint8_t     m_devid[4]; //32 bits device ide
};


#endif /* RDM_UID_H_ */
