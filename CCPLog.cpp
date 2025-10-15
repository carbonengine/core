// Copyright © 2008 CCP ehf.

#include "CCPLog.h"

#include <cassert>
#include <thread>
#include <vector>

#ifdef _WIN32
	#include <windows.h>
#endif

char s_largeBuffer[65535]{};

namespace
{
	char s_lastError[ 256 ] = "";

	struct LogEchoFuncEntry
	{
		CCP::LogEchoFunc func;
		CCP::LogEchoPrivilege privilege;
		bool isThreadSafe;
	};
	typedef std::vector<LogEchoFuncEntry> LogEchoList;
	LogEchoList& GetLogEchos( CCP::LogType type )
	{
		static LogEchoList callbacks[CCP::LOGTYPE_COUNT];
		return callbacks[type];
	}

	std::thread::id s_mainThreadId;

};

namespace CCP
{

bool g_logtypeInfoIsPrivilegedOnly{false};
bool g_logtypeNoticeIsPrivilegedOnly{false};
bool g_logtypeWarnIsPrivilegedOnly{false};
bool g_logtypeErrIsPrivilegedOnly{false};

bool SetLogtypeInfoIsPrivileged(bool privileged)
{
	auto old = g_logtypeInfoIsPrivilegedOnly;
	g_logtypeInfoIsPrivilegedOnly = privileged;
	return old;
}

bool SetLogtypeNoticeIsPrivileged(bool privileged)
{
	auto old = g_logtypeNoticeIsPrivilegedOnly;
	g_logtypeNoticeIsPrivilegedOnly = privileged;
	return old;
}

bool SetLogtypeWarnIsPrivileged(bool privileged)
{
	auto old = g_logtypeWarnIsPrivilegedOnly;
	g_logtypeWarnIsPrivilegedOnly = privileged;
	return old;
}

bool SetLogtypeErrIsPrivileged(bool privileged)
{
	auto old = g_logtypeErrIsPrivilegedOnly;
	g_logtypeErrIsPrivilegedOnly = privileged;
	return old;
}

uint32_t& GetLogCounter( CCP::LogType type )
{
	static uint32_t counters[CCP::LOGTYPE_COUNT] = {0, 0, 0, 0};
	return counters[type];
}

void SetLogMainThreadId()
{
	s_mainThreadId = std::this_thread::get_id();
}

void RegisterLogEcho( LogEchoFunc cb, LogType threshold, bool isThreadSafe, LogEchoPrivilege privilege )
{
	// Ensure we don't get duplicates
	UnregisterLogEcho( cb );

	LogEchoFuncEntry e;
	e.func = cb;
	e.privilege = privilege;
	e.isThreadSafe = isThreadSafe;

	for( unsigned int ix = threshold; ix < LOGTYPE_COUNT; ++ix )
	{
		GetLogEchos( (LogType)ix ).push_back( e );
	}
}

void UnregisterLogEcho( LogEchoFunc cb )
{
	for( unsigned int ix = LOGTYPE_LOWEST; ix < LOGTYPE_COUNT; ++ix )
	{
		LogEchoList& list = GetLogEchos( (LogType)ix );
		LogEchoList::iterator it;
		for( it = list.begin(); it != list.end(); ++it )
		{
			if( it->func == cb )
			{
				break;
			}
		}

		// Note - even if there is only 1 item in the list the logic below will work.
		if( it != list.end() )
		{
			LogEchoFuncEntry backItem = list.back();
			*it = backItem;
			list.pop_back();
		}
	}
}

bool IsLogging( LogType threshold )
{
	return !GetLogEchos( threshold ).empty();
}

static CcpLogChannel_t s_logObject = { 1, "carbon-core", "Main", 0 };

void LogToDebugger( CcpLogChannel_t& logObject, LogType type, unsigned long userData, const char* message )
{
#if defined( _WIN32 )
	static const char* s_logType2string[ CCP::LOGTYPE_COUNT ] = { "[I] ", "[N] ", "[W] ", "[E] " };	
	static char s_buffer[256];
	int rv = _snprintf_s( s_buffer, _TRUNCATE, "%s%s/%s:%s\n", s_logType2string[ (int)type ], logObject.facility, logObject.object, message  );
	OutputDebugString( s_buffer );
	if( rv < 0 )
	{
		// output was truncated - emit the newline character
		OutputDebugString( "...\n" );
	}
#endif
}

CARBON_CORE_API void LogFuncChannel( CcpLogChannel_t& logObject, LogType type, unsigned long userData, CCPLOG_PRINTF_FORMAT const char* format, ... )
{
	va_list args;
	va_start( args, format );
	LogFuncChannel_v( logObject, type, userData, format, args );
	va_end( args );
}


void LogFuncChannelRaw( CcpLogChannel_t& logObject, LogType type, unsigned long userData, const char* text )
{
	LogEchoList& callbacks = GetLogEchos( type );

	LogEchoList::iterator end = callbacks.end();
	for( LogEchoList::iterator it = callbacks.begin(); it != end; ++it )
	{
		if( it->isThreadSafe || (std::this_thread::get_id() == s_mainThreadId) )
		{
			if (it->privilege == LOG_ECHO_REQUIRES_PRIVILEGE_CHECK)
			{
				if (type == LOGTYPE_INFO && g_logtypeInfoIsPrivilegedOnly)
					continue;

				if (type == LOGTYPE_NOTICE && g_logtypeNoticeIsPrivilegedOnly)
					continue;

				if (type == LOGTYPE_WARN && g_logtypeWarnIsPrivilegedOnly)
					continue;

				if (type == LOGTYPE_ERR && g_logtypeErrIsPrivilegedOnly)
					continue;
			}

			(it->func)( logObject, type, userData, text );
		}
	}

	// Copy the message if it's an error. Note that writing directly to the
	// s_lastError (above) would be unsafe (since one of the varargs could be s_lastError).
	if( type == LOGTYPE_ERR )
	{
		strncpy( s_lastError, text, std::extent<decltype(s_lastError)>::value - 1 );
	}
}

CARBON_CORE_API void LogFuncChannel_v( CcpLogChannel_t& logObject, LogType type, unsigned long userData, const char* format, va_list args )
{
	if( !format )
	{
		return;
	}
	GetLogCounter( type ) += 1;

	LogEchoList& callbacks = GetLogEchos( type );
	if( (type != LOGTYPE_ERR) && callbacks.empty() )
	{
		// There are no callbacks so the log wouldn't go anywhere. Note that we do
		// continue with errors - this is so we store the last logged error even
		// though it won't be output.
		return;
	}

    va_list argCopy1, argCopy2;
#ifdef _MSC_VER
    argCopy1 = args;
    argCopy2 = args;
#else
    va_copy( argCopy1, args );
    va_copy( argCopy2, args );
#endif
    
	char localBuffer[256];
	char* output = localBuffer;
	if ( vsnprintf( localBuffer, std::extent_v<decltype(localBuffer)>, format, args ) > ( std::extent_v<decltype(localBuffer)> - 1 ) )
	{
		int size = vsnprintf( nullptr, 0, format, argCopy1 );
		if( size < 1 )
		{
#ifndef _MSC_VER
			va_end( argCopy1 );
			va_end( argCopy2 );
#endif
			return;
		}

		output = s_largeBuffer;
		vsnprintf( output, std::extent_v<decltype(s_largeBuffer)>, format, argCopy2 );
		assert( s_largeBuffer[65534] == '\0' );
	}

	LogFuncChannelRaw( logObject, type, userData, output );

#ifndef _MSC_VER
	va_end( argCopy1 );
	va_end( argCopy2 );
#endif
}

void LogFunc( LogType type, unsigned long userData, CCPLOG_PRINTF_FORMAT const char* format, ... )
{
	va_list args;
	va_start( args, format );
	LogFunc_v( type, userData, format, args );
	va_end( args );
}

void LogFunc_v( LogType type, unsigned long userData, const char* format, va_list args )
{
	LogFuncChannel_v( s_logObject, type, userData, format, args );
}

const char* GetLastErrorMessage()
{
	return s_lastError;
}

// End CCP namespace
}
