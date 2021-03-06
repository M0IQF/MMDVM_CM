/*
 *   Copyright (C) 2006-2016 by Jonathan Naylor G4KLX
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

#include "UDPSocket.h"
#include "Log.h"
#include <cassert>
#include <cerrno>
#include <cstring>


CUDPSocket::CUDPSocket(const std::string& address, unsigned int port) :
m_address(address),
m_port(port),
m_fd(-1)
{
	assert(!address.empty());
}

CUDPSocket::CUDPSocket(unsigned int port) :
m_address(),
m_port(port),
m_fd(-1)
{

}

CUDPSocket::~CUDPSocket()
{

}

in_addr CUDPSocket::lookup(const std::string& hostname)
{
	in_addr addr;

	in_addr_t address = ::inet_addr(hostname.c_str());
	if (address != in_addr_t(-1)) {
		addr.s_addr = address;
		LogInfo("inet_addr() returns %x", addr.s_addr);
		return addr;
	}

	struct hostent* hp = ::gethostbyname(hostname.c_str());
	if (hp != NULL) {
		::memcpy(&addr, hp->h_addr_list[0], sizeof(struct in_addr));
		LogInfo("gethostbyname() returns %x", addr.s_addr);
		return addr;
	}

	LogError("Cannot find address for host %s", hostname.c_str());

	addr.s_addr = INADDR_NONE;
	return addr;
}

bool CUDPSocket::open()
{
	m_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
	if (m_fd < 0) {

		LogError("Cannot create the UDP socket, err: %d", errno);
		return false;
	}

	if (m_port > 0U) {
		sockaddr_in addr;
		::memset(&addr, 0x00, sizeof(sockaddr_in));
		addr.sin_family      = AF_INET;
		addr.sin_port        = htons(m_port);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);

		if (!m_address.empty()) {

			addr.sin_addr.s_addr = ::inet_addr(m_address.c_str());

			if (addr.sin_addr.s_addr == INADDR_NONE) {
				LogError("The local address is invalid - %s", m_address.c_str());
				return false;
			}
		}

		int reuse = 1;
		if (::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) == -1) {

			LogError("Cannot set the UDP socket option, err: %d", errno);

			return false;
		}

		if (::bind(m_fd, (sockaddr*)&addr, sizeof(sockaddr_in)) == -1) {

			LogError("Cannot bind the UDP address, err: %d", errno);

			return false;
		}
	}

	return true;
}

int CUDPSocket::read(unsigned char* buffer, unsigned int length, in_addr& address, unsigned int& port)
{
	assert(buffer != NULL);
	assert(length > 0U);

	// Check that the readfrom() won't block
	fd_set readFds;
	FD_ZERO(&readFds);
#if defined(_WIN32) || defined(_WIN64)
	FD_SET((unsigned int)m_fd, &readFds);
#else
	FD_SET(m_fd, &readFds);
#endif

	// Return immediately
	timeval tv;
	tv.tv_sec  = 0L;
	tv.tv_usec = 0L;

	int ret = ::select(m_fd + 1, &readFds, NULL, NULL, &tv);
	if (ret < 0) {
#if defined(_WIN32) || defined(_WIN64)
		LogError("Error returned from UDP select, err: %lu", ::GetLastError());
#else
		LogError("Error returned from UDP select, err: %d", errno);
#endif
		return -1;
	}

	if (ret == 0)
		return 0;

	sockaddr_in addr;
#if defined(_WIN32) || defined(_WIN64)
	int size = sizeof(sockaddr_in);
#else
	socklen_t size = sizeof(sockaddr_in);
#endif

#if defined(_WIN32) || defined(_WIN64)
	int len = ::recvfrom(m_fd, (char*)buffer, length, 0, (sockaddr *)&addr, &size);
#else
	ssize_t len = ::recvfrom(m_fd, (char*)buffer, length, 0, (sockaddr *)&addr, &size);
#endif
	if (len <= 0) {
#if defined(_WIN32) || defined(_WIN64)
		LogError("Error returned from recvfrom, err: %lu", ::GetLastError());
#else
		LogError("Error returned from recvfrom, err: %d", errno);
#endif
		return -1;
	}

	address = addr.sin_addr;
	port    = ntohs(addr.sin_port);

	return len;
}

bool CUDPSocket::write(const unsigned char* buffer, unsigned int length, const in_addr& address, unsigned int port)
{
	assert(buffer != NULL);
	assert(length > 0U);

	sockaddr_in addr;
	::memset(&addr, 0x00, sizeof(sockaddr_in));

	addr.sin_family = AF_INET;
	addr.sin_addr   = address;
	addr.sin_port   = htons(port);


	ssize_t ret = ::sendto(m_fd, (char *)buffer, length, 0, (sockaddr *)&addr, sizeof(sockaddr_in));

	if (ret < 0) {

		LogError("Error returned from sendto, err: %d", errno);

		return false;
	}


	if (ret != ssize_t(length))
		return false;


	return true;
}

void CUDPSocket::close()
{
#if defined(_WIN32) || defined(_WIN64)
	::closesocket(m_fd);
#else
	::close(m_fd);
#endif
}
