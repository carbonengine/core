// Copyright © 2025 CCP ehf.

#ifndef _CCPASSERT_H_
#define _CCPASSERT_H_

#include "carbon_core_export.h"

// Helpers for implementing CCP_ASSERT. These helpers need to be available in Blue regardless
// the setting of CCP_ASSERT_ENABLED in Blue itself, as other modules linking against Blue
// may have assertions enabled even if Blue itself does not.

enum CCPAssertResult
{
    CCP_ASSERT_RESULT_NONE,
    CCP_ASSERT_RESULT_CONTINUE,
    CCP_ASSERT_RESULT_IGNORE,
    CCP_ASSERT_RESULT_BREAK
};

/// The assert helper class is instantiated for each assertion and keeps a flag to allow ignoring
/// future assertion fails in the given location.
class CCPAssertHelper
{
public:
    CCPAssertHelper() : m_isIgnored( false ) {}

    CARBON_CORE_API CCPAssertResult Handle( const char* pMsg, const char* pFile, int line );

private:
    bool m_isIgnored;
};

CARBON_CORE_API bool CcpIsDebuggerPresent();

//////////////////////////////////////////////////////////////////////////////////////////////////
///
/// Assertion macros
///

/// CCP_ASSERT_ENABLED controls whether assertion macros are enabled or not. By default they are enabled
/// in debug builds and not enabled in release builds, but can be explicitly turned on/off
/// by defining CCP_ASSERT_ENABLED and setting to 0 or 1.
#if !defined( CCP_ASSERT_ENABLED )
	#if NDEBUG
		#define CCP_ASSERT_ENABLED 0
	#else
		#define CCP_ASSERT_ENABLED 1
	#endif
#endif

// TODO: Does this exist already? Do we need a separate define controlling whether this does something or not?
#ifdef _MSC_VER
	#define CCP_DEBUG_BREAK() __debugbreak()
#else
	#include <signal.h>
    #if defined(__APPLE__)
        #define CCP_DEBUG_BREAK() { if( CcpIsDebuggerPresent() ) raise(SIGTRAP); }
	#elif defined(SIGTRAP)
		#define CCP_DEBUG_BREAK() raise(SIGTRAP)
	#else
		#define CCP_DEBUG_BREAK() raise(SIGABRT)
	#endif
#endif


#if CCP_ASSERT_ENABLED

/// This assert macro has the condition separate from the message, allowing a custom
/// message to be displayed when the condition fails. The typical assert, with the
/// message being simply the text representing the condition is defined in terms
/// of this macro.
///
/// The do-while(0) idiom is used to make the assert a legal statement:
/// if( x )
///		CCP_ASSERT( y );
/// else
///		...
///
/// Without the else in the assert macro the above code would give you an error due to
/// mismatching else/if. Then again, you should always use the curly braces...

#define CCP_ASSERT_M( cond, msg ) do { \
	if( !(cond) ) \
	{ \
		static CCPAssertHelper sHelper; \
		if( sHelper.Handle( msg, __FILE__, __LINE__ ) == CCP_ASSERT_RESULT_BREAK ) \
		{ \
			CCP_DEBUG_BREAK(); /* Want to do this in the macro so it stops on the right line*/ \
		} \
	} \
} while (0)

#define CCP_ASSERT( cond )			CCP_ASSERT_M( cond, #cond )

//
// Check macro, similar to assert, but returns from the calling function if condition fails
//
#define CCP_CHECK_RETURN( cond, ... ) do { \
	if( !(cond) ) \
	{ \
		static CCPAssertHelper sHelper; \
		if( sHelper.Handle( #cond, __FILE__, __LINE__ ) == CCP_ASSERT_RESULT_BREAK ) \
		{ \
			CCP_DEBUG_BREAK(); /* Want to do this in the macro so it stops on the right line*/ \
		} \
		return __VA_ARGS__; \
	} \
} while (0)

#else // CCP_ASSERT_ENABLED

/// Assert macros defined away to nothing if CCP_ASSERT_ENABLED is false

#define CCP_ASSERT( cond )
#define CCP_ASSERT_M( cond, msg )

// Check macros silently check the condition
#define CCP_CHECK_RETURN( cond, ... )	do { \
	if( !(cond) ) \
	{ \
		return __VA_ARGS__; \
	} \
} while (0)

#endif

///
//////////////////////////////////////////////////////////////////////////////////////////////////


#endif
