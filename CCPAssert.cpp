// Copyright © 2025 CCP ehf.

#include "include/CCPAssert.h"
#include "include/CcpSecureCrt.h"

#ifdef _WIN32
	#include <winuser.h>
#endif

#if _MSC_VER
	#include <crtdbg.h>
#endif

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif


#define ASSERT_USE_THREAD 0

#ifdef _WIN32

///
/// The CCPAssertDialogThread helper class is used to display a standard Windows message box
/// on a separate thread. This allows us (in theory) to display the dialog box regardless of
/// the state of the application (and its message queue).
///
class CCPAssertDialogThread
{
public:
	CCPAssertDialogThread( const char* pMsg ) :
		m_pMsg( pMsg ),
		m_result( CCP_ASSERT_RESULT_NONE )
	{
	}

    CCPAssertResult Run()
	{
		m_result = CCP_ASSERT_RESULT_NONE;

#if ASSERT_USE_THREAD
		HANDLE handle = CreateThread( 0, 64*1024, MessageBoxThreadProc, this, 0, 0 );
		if (handle)
		{
			WaitForSingleObject( handle, INFINITE );
			CloseHandle( handle );
		}
#else
		MessageBoxThreadProc( (void*)this );
#endif

		return m_result;
	}

    static unsigned long __stdcall MessageBoxThreadProc( void* p )
	{
		CCPAssertDialogThread* pThis = static_cast<CCPAssertDialogThread*>( p );

		MSG msg;
		PeekMessage( &msg, NULL, WM_QUIT, WM_QUIT, PM_REMOVE );

		const int mbResult = MessageBox( 0, pThis->m_pMsg, "Assertion failed", MB_ABORTRETRYIGNORE | MB_DEFBUTTON2 | MB_ICONWARNING | MB_TASKMODAL | MB_SETFOREGROUND);

		switch (mbResult)
		{
			case IDABORT:
				pThis->m_result = CCP_ASSERT_RESULT_BREAK;
				break;

			case IDRETRY:
				pThis->m_result = CCP_ASSERT_RESULT_CONTINUE;
				break;

			case IDIGNORE:
				pThis->m_result = CCP_ASSERT_RESULT_IGNORE;
				break;  
		}

		return 0;
	}

private:
    const char* m_pMsg;
    CCPAssertResult m_result;
};

#else // _WIN32

class CCPAssertDialogThread
{
public:
	CCPAssertDialogThread( const char* pMsg )
    :   m_msg( pMsg )
    {
    }
	CCPAssertResult Run()
	{
#ifdef __APPLE__
        const char* title = "Assertion Failed";
        
        CFStringRef messageRef = CFStringCreateWithCString( nullptr, m_msg, kCFStringEncodingASCII );
        
        CFOptionFlags result;  //result code from the message box
        
        CFUserNotificationDisplayAlert( 0, kCFUserNotificationStopAlertLevel, nullptr, nullptr, nullptr, CFSTR( "Assertion Failed" ), messageRef, CFSTR( "Retry" ), CFSTR( "Ignore" ), CFSTR( "Abort" ), &result );
        
        CFRelease( messageRef );
        
        //Convert the result
        switch( result )
        {
        case kCFUserNotificationDefaultResponse:
            return CCP_ASSERT_RESULT_CONTINUE;
        case kCFUserNotificationOtherResponse:
            return CCP_ASSERT_RESULT_BREAK;
        default:
            return CCP_ASSERT_RESULT_IGNORE;
        }
#else
		return CCP_ASSERT_RESULT_BREAK;
#endif
	}
private:
    const char* m_msg;
};

void OutputDebugString( const char* msg )
{
	fprintf( stderr, "%s", msg );
}

#endif // _WIN32

CCPAssertResult CCPAssertHelper::Handle( const char* pMsg, const char* pFile, int line )
{
	if( m_isIgnored )
	{
		return CCP_ASSERT_RESULT_IGNORE;
	}

	const int kAssertMsgMaxLength = 512;
	char assertMsg[ kAssertMsgMaxLength ];
	_snprintf_s( assertMsg, kAssertMsgMaxLength, "Assertion failed in file %s at line %d:\n'%s'\n", pFile, line, pMsg );
	OutputDebugString( assertMsg );

	CCPAssertDialogThread dlgThread( assertMsg );
	CCPAssertResult result = dlgThread.Run();

	if( result == CCP_ASSERT_RESULT_IGNORE )
	{
		m_isIgnored = true;
	}

	return result;
}

#ifdef __APPLE__

#include <sys/types.h>
#include <unistd.h>
#include <sys/sysctl.h>

// From https://developer.apple.com/library/mac/qa/qa1361/_index.html
bool CcpIsDebuggerPresent()
{
    int                 junk;
    int                 mib[4];
    struct kinfo_proc   info;
    size_t              size;
    
    // Initialize the flags so that, if sysctl fails for some bizarre
    // reason, we get a predictable result.
    
    info.kp_proc.p_flag = 0;
    
    // Initialize mib, which tells sysctl the info we want, in this case
    // we're looking for information about a specific process ID.
    
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();
    
    // Call sysctl.
    
    size = sizeof(info);
    junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
    assert(junk == 0);
	(void)junk;
    
    // We're being debugged if the P_TRACED flag is set.
    
    return ( (info.kp_proc.p_flag & P_TRACED) != 0 );
}

#elif _WIN32

bool CcpIsDebuggerPresent()
{
    return IsDebuggerPresent() != 0;
}

#endif

