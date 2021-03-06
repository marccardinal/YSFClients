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

#if !defined(Reflectors_H)
#define	Reflectors_H

#include "UDPSocket.h"
#include "Timer.h"
#include "Hosts.h"

#include <string>

class CYSFReflector {
public:
	CYSFReflector() :
	m_id(),
	m_name(),
	m_desc(),
	m_count("000"),
	m_address(),
	m_port(0U),
	m_timer(1000U, 700U)
	{
	}

	std::string  m_id;
	std::string  m_name;
	std::string  m_desc;
	std::string  m_count;
	in_addr      m_address;
	unsigned int m_port;
	CTimer       m_timer;
};

class CReflectors {
public:
	CReflectors(const std::string& hostsFile, unsigned int statusPort);
	~CReflectors();

	bool load();

	CYSFReflector* find(const std::string& id);

	std::vector<CYSFReflector*>& current();

	void clock(unsigned int ms);

private:
	std::string                 m_hostsFile;
	CUDPSocket                  m_socket;
	std::vector<CYSFReflector*> m_reflectors;
	std::vector <CYSFReflector*>::iterator m_it;
	std::vector<CYSFReflector*> m_current;
	CTimer                      m_timer;
};

#endif
