
This library has been developed to support the CTC-DRA-13-1 Isolated DMX-RDM Shield ans CTC-DRA-10-1 No Isolated Shield on the Arduino platform. 
That being said does not mean that the library won't work with other interfaces. This library works on the principle of using a RS485 driver to drive a DMX line or act as a receiver ( DMX Slave )

If you wish to make additions or find bugs or would like to contribute in any other way then please don't hesitate to contact me via my email address: danny@illogic.nl or use report issues on the sourceforge project page instead.


For information on installing libraries, see: http://arduino.cc/en/Guide/Libraries


*** COPYRIGHT STATEMENT ***

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

CHANGE LOG:

    - xx-xxx-xxxx: Add basic Remote Device Management support
    - 14-apr-2013: Fixed startbyte recognition
    - 03-apr-2013: Fixed memomry allocation in slave
