/*
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#if !defined(GPS_H)
#define	GPS_H

#include <string>

class CGPS {
public:
	CGPS(const std::string& hostname, unsigned int port, const std::string& password);
	~CGPS();

	void data(const unsigned char* source, const unsigned char* data, unsigned char fi, unsigned char dt, unsigned char fn);

	void clock(unsigned int ms);

	void reset();

private:
	std::string    m_hostname;
	unsigned int   m_port;
	std::string    m_password;
	unsigned char* m_buffer;
	bool           m_dt1;
	bool           m_dt2;
	bool           m_sent;

	void transmitGPS(const unsigned char* source);
};

#endif
