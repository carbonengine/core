// Copyright © 2015 CCP ehf.

#include "include/CcpThread.h"

// CcpThread implementation cannot reside in CcpThread.cpp file because we
// cannot mix BLUEIMPORT and statically-linked functions in a single cpp file.

#if defined(_MSC_VER) && _MSC_VER <= 1700

CcpThread::CcpThread()
	:m_thread( nullptr ),
	m_id( 0 )
{
}

CcpThread::CcpThread( CcpThread&& other )
	:m_thread( other.m_thread ),
	m_id( other.m_id )
{
	other.m_thread = nullptr;
	other.m_id = 0;
}

CcpThread::~CcpThread()
{
	if( joinable() )
	{
		std::terminate();
	}
}

CcpThread& CcpThread::operator=( CcpThread&& other )
{
	if( joinable() )
	{
		std::terminate();
	}
	if( &other == this )
	{
		return *this;
	}
	m_thread = other.m_thread;
	m_id = other.m_id;
	other.m_thread = nullptr;
	other.m_id = 0;
	return *this;
}

bool CcpThread::joinable() const
{
	return m_thread != nullptr;
}

DWORD CcpThread::get_id() const
{
	return m_id;
}

HANDLE CcpThread::native_handle()
{
	return m_thread;
}

void CcpThread::join()
{
	if( joinable() )
	{
		WaitForSingleObject( m_thread, INFINITE );
		CloseHandle( m_thread );
		m_thread = nullptr;
	}
}

void CcpThread::detach()
{
	if( joinable() )
	{
		CloseHandle( m_thread );
		m_thread = nullptr;
	}
}

void CcpThread::swap( CcpThread& other )
{
	std::swap( m_thread, other.m_thread );
}

#endif
