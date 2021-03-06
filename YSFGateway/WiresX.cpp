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

#include "WiresX.h"
#include "YSFPayload.h"
#include "YSFFICH.h"
#include "Utils.h"
#include "Sync.h"
#include "CRC.h"
#include "Log.h"

#include <cstdio>
#include <cassert>

const unsigned char DX_REQ[]    = {0x5DU, 0x71U, 0x5FU};
const unsigned char CONN_REQ[]  = {0x5DU, 0x23U, 0x5FU};
const unsigned char DISC_REQ[]  = {0x5DU, 0x2AU, 0x5FU};
const unsigned char ALL_REQ[]   = {0x5DU, 0x66U, 0x5FU};

const unsigned char DX_RESP[]   = {0x5DU, 0x51U, 0x5FU, 0x26U};
const unsigned char CONN_RESP[] = {0x5DU, 0x41U, 0x5FU, 0x26U};
const unsigned char DISC_RESP[] = {0x5DU, 0x41U, 0x5FU, 0x26U};
const unsigned char ALL_RESP[]  = {0x5DU, 0x46U, 0x5FU, 0x26U};

const unsigned char DEFAULT_FICH[] = {0x20U, 0x00U, 0x01U, 0x00U};

const unsigned char NET_HEADER[] = "YSFDGATEWAY             ALL      ";

CWiresX::CWiresX(const std::string& callsign, CNetwork* network, const std::string& hostsFile, unsigned int statusPort) :
m_callsign(callsign),
m_network(network),
m_reflectors(hostsFile, statusPort),
m_reflector(NULL),
m_id(),
m_name(),
m_txFrequency(0U),
m_rxFrequency(0U),
m_timer(1000U, 1U),
m_seqNo(0U),
m_header(NULL),
m_source(NULL),
m_csd1(NULL),
m_csd2(NULL),
m_csd3(NULL),
m_status(WXSI_NONE)
{
	assert(network != NULL);
	assert(statusPort > 0U);

	m_callsign.resize(YSF_CALLSIGN_LENGTH, ' ');

	m_header = new unsigned char[34U];
	m_source = new unsigned char[20U];
	m_csd1   = new unsigned char[20U];
	m_csd2   = new unsigned char[20U];
	m_csd3   = new unsigned char[20U];
}

CWiresX::~CWiresX()
{
	delete[] m_csd3;
	delete[] m_csd2;
	delete[] m_csd1;
	delete[] m_source;
	delete[] m_header;
}

void CWiresX::setInfo(const std::string& name, unsigned int txFrequency, unsigned int rxFrequency)
{
	assert(txFrequency > 0U);
	assert(rxFrequency > 0U);

	m_name        = name;
	m_txFrequency = txFrequency;
	m_rxFrequency = rxFrequency;

	m_name.resize(14U, ' ');

	unsigned int hash = 0U;

	for (unsigned int i = 0U; i < name.size(); i++) {
		hash += name.at(i);
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	// Final avalanche
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	char id[10U];
	::sprintf(id, "%05u", hash % 100000U);

	LogInfo("The ID of this repeater is %s", id);

	m_id = std::string(id);

	::memset(m_csd1, '*', 20U);
	::memset(m_csd2, ' ', 20U);
	::memset(m_csd3, ' ', 20U);

	for (unsigned int i = 0U; i < 10U; i++) {
		m_csd1[i + 10U] = m_callsign.at(i);
		m_csd2[i + 0U]  = m_callsign.at(i);
	}

	for (unsigned int i = 0U; i < 5U; i++) {
		m_csd3[i + 0U]  = m_id.at(i);
		m_csd3[i + 15U] = m_id.at(i);
	}

	for (unsigned int i = 0U; i < 34U; i++)
		m_header[i] = NET_HEADER[i];

	for (unsigned int i = 0U; i < 10U; i++)
		m_header[i + 14U] = m_callsign.at(i);
}

bool CWiresX::start()
{
	return m_reflectors.load();
}

WX_STATUS CWiresX::process(const unsigned char* data, unsigned char fi, unsigned char dt, unsigned char fn)
{
	assert(data != NULL);

	if (dt != YSF_DT_DATA_FR_MODE)
		return WXS_NONE;

	CYSFPayload payload;

	if (fi == YSF_FI_HEADER) {
		payload.readDataFRModeData1(data, m_source);
		return WXS_NONE;
	}

	if (fi == YSF_FI_COMMUNICATIONS && fn == 0U) {
		if (::memcmp(m_source, "                    ", 20U) == 0)
			payload.readDataFRModeData1(data, m_source);
		return WXS_NONE;
	}

	if (fi == YSF_FI_COMMUNICATIONS && fn == 1U) {
		unsigned char buffer[20U];
		bool valid = payload.readDataFRModeData2(data, buffer);
		if (!valid) {
			::memset(m_source, ' ', 20U);
			return WXS_NONE;
		}

		if (::memcmp(buffer + 1U, DX_REQ, 3U) == 0) {
			processDX();
			return WXS_NONE;
		} else if (::memcmp(buffer + 1U, ALL_REQ, 3U) == 0) {
			processAll();
			return WXS_NONE;
		} else if (::memcmp(buffer + 1U, CONN_REQ, 3U) == 0) {
			return processConnect(buffer + 5U);
		} else if (::memcmp(buffer + 1U, DISC_REQ, 3U) == 0) {
			processDisconnect();
			return WXS_DISCONNECT;
		} else {
			::memset(m_source, ' ', 20U);
			return WXS_NONE;
		}
	}

	return WXS_NONE;
}

CYSFReflector* CWiresX::getReflector() const
{
	return m_reflector;
}

void CWiresX::processDX()
{
	::LogDebug("Received DX from %10.10s", m_source + 10U);

	m_status = WXSI_DX;
	m_timer.start();
}

void CWiresX::processAll()
{
	::LogDebug("Received ALL from %10.10s", m_source + 10U);

	m_status = WXSI_ALL;
	m_timer.start();
}

WX_STATUS CWiresX::processConnect(const unsigned char* data)
{
	::LogDebug("Received Connect to %5.5s from %10.10s", data, m_source + 10U);

	std::string id = std::string((char*)data, 5U);

	m_reflector = m_reflectors.find(id);
	if (m_reflector == NULL)
		return WXS_NONE;

	m_status = WXSI_CONNECT;
	m_timer.start();

	return WXS_CONNECT;
}

void CWiresX::processDisconnect()
{
	::LogDebug("Received Disconect from %10.10s", m_source + 10U);

	m_status = WXSI_DISCONNECT;
	m_timer.start();
}

void CWiresX::clock(unsigned int ms)
{
	m_reflectors.clock(ms);

	m_timer.clock(ms);
	if (m_timer.isRunning() && m_timer.hasExpired()) {
		switch (m_status) {
		case WXSI_DX:
			sendDXReply();
			break;
		case WXSI_ALL:
			sendAllReply();
			break;
		case WXSI_CONNECT:
			sendConnectReply();
			break;
		case WXSI_DISCONNECT:
			sendDisconnectReply();
			break;
		default:
			break;
		}

		m_status = WXSI_NONE;
		m_timer.stop();
	}
}

void CWiresX::createReply(const unsigned char* data, unsigned int length)
{
	assert(data != NULL);
	assert(length > 0U);

	unsigned char ft = calculateFT(length);
	unsigned char bt = length / 260U;

	// Write the header
	unsigned char buffer[200U];
	::memcpy(buffer, m_header, 34U);
	buffer[34U] = 0x00U;

	CSync::add(buffer + 35U);

	CYSFFICH fich;
	fich.load(DEFAULT_FICH);
	fich.setFI(YSF_FI_HEADER);
	fich.setBT(bt);
	fich.setFT(ft);
	fich.encode(buffer + 35U);

	CYSFPayload payload;
	payload.writeDataFRModeData1(m_csd1, buffer + 35U);
	payload.writeDataFRModeData2(m_csd2, buffer + 35U);

	m_network->write(buffer);

	fich.setFI(YSF_FI_COMMUNICATIONS);

	unsigned char fn = 0U;
	unsigned char bn = 0U;

	unsigned int offset = 0U;
	while (offset < length) {
		switch (fn) {
		case 0U: {
				unsigned int len = length - offset;
				ft = calculateFT(len);
				payload.writeDataFRModeData1(m_csd1, buffer + 35U);
				payload.writeDataFRModeData2(m_csd2, buffer + 35U);
			}
			break;
		case 1U:
			payload.writeDataFRModeData1(m_csd3, buffer + 35U);
			payload.writeDataFRModeData2(data + offset, buffer + 35U);
			offset += 20U;
			break;
		default:
			payload.writeDataFRModeData1(data + offset, buffer + 35U);
			offset += 20U;
			payload.writeDataFRModeData2(data + offset, buffer + 35U);
			offset += 20U;
			break;
		}

		fich.setFT(ft);
		fich.setFN(fn);
		fich.setBT(bt);
		fich.setBN(bn);
		fich.encode(buffer + 35U);

		m_network->write(buffer);

		fn++;
		if (fn >= 8U) {
			fn = 0U;
			bn++;
		}
	}

	// Write the trailer
	buffer[34U] = 0x01U;

	fich.setFI(YSF_FI_TERMINATOR);
	fich.setFN(fn);
	fich.setBN(bn);
	fich.encode(buffer + 35U);

	payload.writeDataFRModeData1(m_csd1, buffer + 35U);
	payload.writeDataFRModeData2(m_csd2, buffer + 35U);

	m_network->write(buffer);
}

unsigned char CWiresX::calculateFT(unsigned int length) const
{
	if (length > 220U) return 7U;

	if (length > 180U) return 6U;

	if (length > 140U) return 5U;

	if (length > 100U) return 4U;

	if (length > 60U)  return 3U;

	if (length > 20U)  return 2U;

	return 1U;
}

void CWiresX::sendDXReply()
{
	unsigned char data[150U];
	::memset(data, 0x00U, 150U);
	::memset(data, ' ', 128U);

	data[0U] = m_seqNo;

	for (unsigned int i = 0U; i < 4U; i++)
		data[i + 1U] = DX_RESP[i];

	for (unsigned int i = 0U; i < 5U; i++)
		data[i + 5U] = m_id.at(i);

	for (unsigned int i = 0U; i < 10U; i++)
		data[i + 10U] = m_callsign.at(i);

	for (unsigned int i = 0U; i < 14U; i++)
		data[i + 20U] = m_name.at(i);

	data[34U] = '1';
	data[35U] = '2';

	data[57U] = '0';
	data[58U] = '0';
	data[59U] = '0';

	unsigned int offset;
	char sign;
	if (m_txFrequency >= m_rxFrequency) {
		offset = m_txFrequency - m_rxFrequency;
		sign = '-';
	} else {
		offset = m_rxFrequency - m_txFrequency;
		sign = '+';
	}

	char freq[30U];
	::sprintf(freq, "%05u.%06u%c%03u.%06u", m_txFrequency / 1000000U, m_txFrequency % 1000000U, sign, offset / 1000000U, offset % 1000000U);

	for (unsigned int i = 0U; i < 23U; i++)
		data[i + 84U] = freq[i];

	data[127U] = 0x03U;			// End of data marker
	data[128U] = CCRC::addCRC(data, 128U);

	CUtils::dump(1U, "DX Reply", data, 140U);

	createReply(data, 140U);

	m_seqNo++;
}

void CWiresX::sendConnectReply()
{
	assert(m_reflector != NULL);

	unsigned char data[110U];
	::memset(data, 0x00U, 110U);
	::memset(data, ' ', 90U);

	data[0U] = m_seqNo;

	for (unsigned int i = 0U; i < 4U; i++)
		data[i + 1U] = CONN_RESP[i];

	for (unsigned int i = 0U; i < 5U; i++)
		data[i + 5U] = m_id.at(i);

	for (unsigned int i = 0U; i < 10U; i++)
		data[i + 10U] = m_callsign.at(i);

	for (unsigned int i = 0U; i < 14U; i++)
		data[i + 20U] = m_name.at(i);

	data[34U] = '1';
	data[35U] = '5';

	for (unsigned int i = 0U; i < 5U; i++)
		data[i + 36U] = m_reflector->m_id.at(i);

	for (unsigned int i = 0U; i < 16U; i++)
		data[i + 41U] = m_reflector->m_name.at(i);

	for (unsigned int i = 0U; i < 3U; i++)
		data[i + 57U] = m_reflector->m_count.at(i);

	for (unsigned int i = 0U; i < 14U; i++)
		data[i + 70U] = m_reflector->m_desc.at(i);

	data[84U] = '0';
	data[85U] = '0';
	data[86U] = '0';
	data[87U] = '0';
	data[88U] = '0';

	data[89U] = 0x03U;			// End of data marker
	data[90U] = CCRC::addCRC(data, 90U);

	CUtils::dump(1U, "CONNECT Reply", data, 100U);

	createReply(data, 100U);

	m_seqNo++;
}

void CWiresX::sendDisconnectReply()
{
	unsigned char data[110U];
	::memset(data, 0x00U, 110U);
	::memset(data, ' ', 90U);

	data[0U] = m_seqNo;

	for (unsigned int i = 0U; i < 4U; i++)
		data[i + 1U] = DISC_RESP[i];

	for (unsigned int i = 0U; i < 5U; i++)
		data[i + 5U] = m_id.at(i);

	for (unsigned int i = 0U; i < 10U; i++)
		data[i + 10U] = m_callsign.at(i);

	for (unsigned int i = 0U; i < 14U; i++)
		data[i + 20U] = m_name.at(i);

	data[34U] = '1';
	data[35U] = '2';

	data[57U] = '0';
	data[58U] = '0';
	data[59U] = '0';

	data[89U] = 0x03U;			// End of data marker
	data[90U] = CCRC::addCRC(data, 90U);

	CUtils::dump(1U, "DISCONNECT Reply", data, 100U);

	createReply(data, 100U);

	m_seqNo++;
}

void CWiresX::sendAllReply()
{
	std::vector<CYSFReflector*>& curr = m_reflectors.current();

	unsigned char data[1100U];
	::memset(data, 0x00U, 1100U);

	data[0U] = m_seqNo;

	for (unsigned int i = 0U; i < 4U; i++)
		data[i + 1U] = ALL_RESP[i];

	data[5U] = '2';
	data[6U] = '1';

	for (unsigned int i = 0U; i < 5U; i++)
		data[i + 7U] = m_id.at(i);

	for (unsigned int i = 0U; i < 10U; i++)
		data[i + 12U] = m_callsign.at(i);

	unsigned int total = curr.size();
	if (total > 999U) total = 999U;

	unsigned int n = curr.size();
	if (n > 20U) n = 20U;

	::sprintf((char*)(data + 22U), "%03u%03u", n, total);

	data[28U] = 0x0DU;

	unsigned int offset = 29U;
	for (unsigned int j = 0U; j < n; j++, offset += 50U) {
		CYSFReflector* refl = curr.at(j);

		data[offset + 0U] = '5';

		for (unsigned int i = 0U; i < 5U; i++)
			data[i + offset + 1U] = refl->m_id.at(i);

		for (unsigned int i = 0U; i < 16U; i++)
			data[i + offset + 6U] = refl->m_name.at(i);

		for (unsigned int i = 0U; i < 3U; i++)
			data[i + offset + 22U] = refl->m_count.at(i);

		for (unsigned int i = 0U; i < 10U; i++)
			data[i + offset + 25U] = ' ';

		for (unsigned int i = 0U; i < 14U; i++)
			data[i + offset + 35U] = refl->m_desc.at(i);

		data[offset + 49U] = 0x0DU;
	}

	data[offset + 0U] = 0x03U;			// End of data marker
	data[offset + 1U] = CCRC::addCRC(data, offset + 1U);

	unsigned int blocks = (offset + 1U) / 20U;
	if ((blocks % 20U) > 0U) blocks++;

	CUtils::dump(1U, "ALL Reply", data, blocks * 20U);

	createReply(data, blocks * 20U);

	m_seqNo++;
}
