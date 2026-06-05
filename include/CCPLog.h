// Copyright © 2001 CCP ehf.

#pragma once
#ifndef CCP_LOG_H
#define CCP_LOG_H

#include <cstdarg>
#include <stdexcept>
#include "carbon_core_export.h"

#if !defined( CCP_LOG_ENABLED )
	// Default behavior is to enable logging of info
	#define CCP_LOG_ENABLED 1
#endif

#if !defined( CCP_LOGNOTICE_ENABLED )
	// Default behavior is to enable logging of notices
	#define CCP_LOGNOTICE_ENABLED 1
#endif

#if !defined( CCP_LOGWARN_ENABLED )
	// Default behavior is to enable logging of warnings
	#define CCP_LOGWARN_ENABLED 1
#endif

#if !defined( CCP_LOGERR_ENABLED )
	// Default behavior is to enable logging of errors
	#define CCP_LOGERR_ENABLED 1
#endif

// Any modules using CCPLog must define a module name
extern const char* g_moduleName;

typedef short TLOGCHANNELID;
typedef long TLOGSOURCEID;

struct CcpLogChannel_t
{
	long oktocall;
	const char *facility;
	const char *object;
	TLOGCHANNELID		channel;			// index into channel buffer
	TLOGSOURCEID		source;				// dynamic unique id for this instance
};

#define CCP_LOG_DEFINE_CHANNEL( desc ) { 1, g_moduleName, desc, 0 }

#ifdef _MSC_VER
#include <sal.h>
#define CCPLOG_PRINTF_FORMAT _Printf_format_string_
#define CCPLOG_PRINTF_FORMAT_ATTR( formatIdx, dotsIdx )
#else
#define CCPLOG_PRINTF_FORMAT
#define CCPLOG_PRINTF_FORMAT_ATTR( formatIdx, dotsIdx ) __attribute__( ( __format__( __printf__, formatIdx, dotsIdx ) ) )
#endif

namespace CCP
{

enum LogType
{
	LOGTYPE_INFO,
	LOGTYPE_NOTICE,
	LOGTYPE_WARN,
	LOGTYPE_ERR,

	LOGTYPE_COUNT,

	LOGTYPE_LOWEST = LOGTYPE_INFO,
	LOGTYPE_HIGHEST = LOGTYPE_ERR
};

CARBON_CORE_API uint32_t& GetLogCounter( CCP::LogType type );

// Logs to a default channel
CARBON_CORE_API void LogFunc( LogType type, unsigned long userData, CCPLOG_PRINTF_FORMAT const char* format, ... ) CCPLOG_PRINTF_FORMAT_ATTR( 3, 4 );
CARBON_CORE_API void LogFunc_v( LogType type, unsigned long userData, const char* format, va_list args );

// Logs to the given channel
CARBON_CORE_API void LogFuncChannel( CcpLogChannel_t& logObject, LogType type, unsigned long userData, CCPLOG_PRINTF_FORMAT const char* format, ... ) CCPLOG_PRINTF_FORMAT_ATTR( 4, 5 );
CARBON_CORE_API void LogFuncChannel_v( CcpLogChannel_t& logObject, LogType type, unsigned long userData, const char* format, va_list args );

// Only logs from the same thread as this are allowed through, unless
// the log functions is tagged as thread safe.
CARBON_CORE_API void SetLogMainThreadId();

inline CcpLogChannel_t& GetModuleChannel()
{
	static CcpLogChannel_t s_moduleChannel = CCP_LOG_DEFINE_CHANNEL( "Main" );
	return s_moduleChannel;
}

};

#if CCP_LOG_ENABLED
	#define CCP_LOG( formatString, ... ) \
		CCP::LogFuncChannel( \
			CCP::GetModuleChannel(), \
			CCP::LOGTYPE_INFO, \
			0, \
			formatString, \
			##__VA_ARGS__ )
	#define CCP_LOG_CH( logObject, formatString, ... ) \
		CCP::LogFuncChannel( \
			logObject, \
			CCP::LOGTYPE_INFO, \
			0, \
			formatString, \
			##__VA_ARGS__ )
#else
	#define CCP_LOG( formatString, ... )
	#define CCP_LOG_CH( logObject, formatString, ... )
#endif

#if CCP_LOGNOTICE_ENABLED
	#define CCP_LOGNOTICE( formatString, ... ) \
		CCP::LogFuncChannel( \
			CCP::GetModuleChannel(), \
			CCP::LOGTYPE_NOTICE, \
			0, \
			formatString, \
			##__VA_ARGS__ )
	#define CCP_LOGNOTICE_CH( logObject, formatString, ... ) \
		CCP::LogFuncChannel( \
			logObject, \
			CCP::LOGTYPE_NOTICE, \
			0, \
			formatString, \
			##__VA_ARGS__ )
#else
	#define CCP_LOGNOTICE( formatString, ... )
	#define CCP_LOGNOTICE_CH( logObject, formatString, ... )
#endif

#if CCP_LOGWARN_ENABLED
	#define CCP_LOGWARN( formatString, ... ) \
		CCP::LogFuncChannel( \
			CCP::GetModuleChannel(), \
			CCP::LOGTYPE_WARN, \
			0, \
			formatString, \
			##__VA_ARGS__ )
	#define CCP_LOGWARN_CH( logObject, formatString, ... ) \
		CCP::LogFuncChannel( \
			logObject, \
			CCP::LOGTYPE_WARN, \
			0, \
			formatString, \
			##__VA_ARGS__ )
#else
	#define CCP_LOGWARN( formatString, ... )
	#define CCP_LOGWARN_CH( logObject, formatString, ... )
#endif

#if CCP_LOGERR_ENABLED
	#define CCP_LOGERR( formatString, ... ) \
		CCP::LogFuncChannel( \
			CCP::GetModuleChannel(), \
			CCP::LOGTYPE_ERR, \
			0, \
			formatString, \
			##__VA_ARGS__ )
	#define CCP_LOGERR_CH( logObject, formatString, ... ) \
		CCP::LogFuncChannel( \
			logObject, \
			CCP::LOGTYPE_ERR, \
			0, \
			formatString, \
			##__VA_ARGS__ )
#else
	#define CCP_LOGERR( formatString, ... )
	#define CCP_LOGERR_CH( logObject, formatString, ... )
#endif

namespace CCP
{

enum LogEchoPrivilege
{
	LOG_ECHO_REQUIRES_PRIVILEGE_CHECK,
	LOG_ECHO_NO_PRIVILEGE_CHECK,
};

// Returns the last error message, or null terminated string if none.
CARBON_CORE_API const char* GetLastErrorMessage();

// Throws a std exception with string 'message' and logs out the 'message'
inline void Throw( const char* message )
{
	CCP_LOGERR( "%s", message );
	CCP_LOGWARN( "Exception thrown" );
	throw std::runtime_error( message );
}

// Throws a std exception with string 'GetLastErrorMessage'.
inline void ThrowLastError()
{
	// No need to output error message since that has already been done.
	CCP_LOGWARN( "Exception thrown" );
	throw std::runtime_error( GetLastErrorMessage() );
}

typedef void (*LogEchoFunc)( CcpLogChannel_t& channel, LogType type, unsigned long userData, const char* message );

// Register a log echo function. Future logging requests will be passed to this function. Note that
// multiple log echo functions can be registered - this adds the function to a list, rather than replacing
// a previously registered function. The function can be removed with UnregisterLogEcho.
// The threshold value indicates the minimum severity required for the message to be output.
// The isThreadSafe flag indicates whether the log echo function is threadsafe - if it is not, it
// only receives messages from the main thread.
CARBON_CORE_API void RegisterLogEcho( LogEchoFunc cb, LogType threshold = LOGTYPE_INFO, bool isThreadSafe = false, LogEchoPrivilege privilege = LOG_ECHO_NO_PRIVILEGE_CHECK );

// Unregisters a previously registered log echo function.
CARBON_CORE_API void UnregisterLogEcho( LogEchoFunc cb );

// Is any log echo function registered for the given threshold?
CARBON_CORE_API bool IsLogging( LogType threshold = LOGTYPE_INFO );

// Let's offer a standard callback (for convenience) that can be registered:
CARBON_CORE_API void LogToDebugger( CcpLogChannel_t& logObject, LogType type, unsigned long userData, const char* message );

// Sets info type logs to be privileged and returns the previous value
CARBON_CORE_API bool SetLogtypeInfoIsPrivileged(bool privileged);

// Sets notice type logs to be privileged and returns the previous value
CARBON_CORE_API bool SetLogtypeNoticeIsPrivileged(bool privileged);

// Sets warn type logs to be privileged and returns the previous value
CARBON_CORE_API bool SetLogtypeWarnIsPrivileged(bool privileged);

// Sets error type logs to be privileged and returns the previous value
CARBON_CORE_API bool SetLogtypeErrIsPrivileged(bool privileged);
};

#endif
