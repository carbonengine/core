// Copyright © 2025 CCP ehf.

#include "include/CCPCallstack.h"

namespace
{
    
    void DumpPointerToFile( size_t codePointer, void* context )
    {
        fprintf( static_cast<FILE*>( context ), ", 0x%p", reinterpret_cast<void*>( codePointer ) );
    }
    
    void DumpPointerAndNameToFile( size_t codePointer, const char* name, void* context )
    {
        fprintf( static_cast<FILE*>( context ), ", 0x%p(%s)", reinterpret_cast<void*>( codePointer ), name );
    }
    
}

#ifdef _MSC_VER

/// Pull in dbghelp to get symbols for callstack
/*
NB: VS2017 warns about the use of an ill-formed typedef in the <dbghelp.h> file,
    but that's how it ships inside "Windows Kits\8.1" so guess we're stuck with it. 
	We can, at least, mute the warning...
*/
#pragma warning(push)
#pragma warning(disable:4091)
#include <dbghelp.h>
#pragma warning(pop)

///
/// The DbgHelp namespace wraps the functions provided in the DbgHelp DLL from Microsoft.
/// The DLL is loaded (and unloaded) as needed, to minimize runtime overhead.
///
namespace DbgHelp
{
	typedef BOOL (__stdcall *SYMINITIALIZE)( HANDLE, LPSTR, BOOL );
	typedef BOOL (__stdcall *SYMCLEANUP)( HANDLE );
	typedef BOOL (__stdcall *SYMFROMADDR)( HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO );
	typedef DWORD (__stdcall *SYMSETOPTIONS)( DWORD );
	typedef BOOL (__stdcall *STACKWALK64)( DWORD, HANDLE, HANDLE, LPSTACKFRAME64, LPVOID,PREAD_PROCESS_MEMORY_ROUTINE64, PFUNCTION_TABLE_ACCESS_ROUTINE64,PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64 );
	typedef LPVOID (__stdcall *SYMFUNCTIONTABLEACCESS64)( HANDLE, DWORD64 );
	typedef DWORD64 (__stdcall *SYMGETMODULEBASE64)( HANDLE, DWORD64 );
	typedef BOOL (__stdcall *SYMSETSEARCHPATH)( HANDLE, PCTSTR );
	typedef BOOL (__stdcall *SYMGETMODULEINFO64)( HANDLE, DWORD64, PIMAGEHLP_MODULE64 );

	SYMINITIALIZE SymInitialize;
	SYMCLEANUP SymCleanup;
	SYMFROMADDR SymFromAddr;
	SYMSETOPTIONS SymSetOptions;
	STACKWALK64 StackWalk64;
	SYMFUNCTIONTABLEACCESS64 SymFunctionTableAccess64;
	SYMGETMODULEBASE64 SymGetModuleBase64;
	SYMSETSEARCHPATH SymSetSearchPath;
	SYMGETMODULEINFO64 SymGetModuleInfo64;

	unsigned int s_initCount;
	HMODULE s_moduleHandle;

	bool Init()
	{
		if( s_initCount )
		{
			++s_initCount;
			return true;
		}

		s_moduleHandle = LoadLibrary( "dbghelp.dll" );
		if( s_moduleHandle )
		{
			SymInitialize = (SYMINITIALIZE)GetProcAddress( s_moduleHandle, "SymInitialize" );
			SymCleanup = (SYMCLEANUP)GetProcAddress( s_moduleHandle, "SymCleanup" );
			SymSetOptions = (SYMSETOPTIONS)GetProcAddress( s_moduleHandle, "SymSetOptions" );
			SymFromAddr = (SYMFROMADDR)GetProcAddress( s_moduleHandle, "SymFromAddr" );
			StackWalk64 = (STACKWALK64)GetProcAddress( s_moduleHandle, "StackWalk64" );
			SymFunctionTableAccess64 = (SYMFUNCTIONTABLEACCESS64)GetProcAddress( s_moduleHandle, "SymFunctionTableAccess64" );
			SymGetModuleBase64 = (SYMGETMODULEBASE64)GetProcAddress( s_moduleHandle, "SymGetModuleBase64" );
			SymSetSearchPath = (SYMSETSEARCHPATH)GetProcAddress( s_moduleHandle, "SymSetSearchPath" );
			SymGetModuleInfo64 = (SYMGETMODULEINFO64)GetProcAddress( s_moduleHandle, "SymGetModuleInfo64" );

			DWORD flags = SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS;// | SYMOPT_LOAD_ANYTHING;
			
			/// To see what DbgHelp attempts to load, enable the following line:
			flags |= SYMOPT_DEBUG;

			SymSetOptions( flags );

			HANDLE hProcess = GetCurrentProcess();
			// HACK: This path is hard-coded - figure out the right way to get the location
			SymInitialize( hProcess, 0, true );

			++s_initCount;
			return true;
		}
		else
		{
			return false;
		}
	}

	void Done()
	{
		if( s_initCount )
		{
			--s_initCount;

			if( !s_initCount )
			{
				FreeLibrary( s_moduleHandle );
			}
		}
	}
}

// _ReturnAddress should be prototyped before use
extern "C" void * _ReturnAddress(void);

#pragma intrinsic(_ReturnAddress)

#pragma auto_inline(off)
DWORD_PTR
GetProgramCounter()
{
	return (DWORD_PTR)_ReturnAddress();
}
#pragma auto_inline(on)

#pragma warning (push)
#pragma warning (disable: 4748)
void CCPCallstack::Capture()
{
	DbgHelp::Init();
	m_depth = 0;

	HANDLE hProcess = GetCurrentProcess();
	HANDLE hThread = GetCurrentThread();

	DWORD machineType;

	CONTEXT context;
	ZeroMemory( &context, sizeof( CONTEXT ) );
	context.ContextFlags = CONTEXT_CONTROL;

	STACKFRAME64 stackFrame; 
	ZeroMemory(&stackFrame, sizeof( STACKFRAME64 ) ); 

#ifdef _WIN64
	RtlCaptureContext( &context );
	#if _M_X64
		machineType                 = IMAGE_FILE_MACHINE_AMD64;
		stackFrame.AddrPC.Offset    = context.Rip;
		stackFrame.AddrPC.Mode      = AddrModeFlat;
		stackFrame.AddrFrame.Offset = context.Rsp;
		stackFrame.AddrFrame.Mode   = AddrModeFlat;
		stackFrame.AddrStack.Offset = context.Rsp;
		stackFrame.AddrStack.Mode   = AddrModeFlat;
	#elif _M_IA64
		machineType                 = IMAGE_FILE_MACHINE_IA64;
		stackFrame.AddrPC.Offset    = context.StIIP;
		stackFrame.AddrPC.Mode      = AddrModeFlat;
		stackFrame.AddrFrame.Offset = context.IntSp;
		stackFrame.AddrFrame.Mode   = AddrModeFlat;
		stackFrame.AddrBStore.Offset= context.RsBSP;
		stackFrame.AddrBStore.Mode  = AddrModeFlat;
		stackFrame.AddrStack.Offset = context.IntSp;
		stackFrame.AddrStack.Mode   = AddrModeFlat;
	#else
		#error "Unsupported platform"
	#endif

#else
	// 32 bit Windows
	machineType = IMAGE_FILE_MACHINE_I386;
	__asm
	{
Label:
		mov [context.Ebp], ebp;
		mov [context.Esp], esp;
		mov eax, [Label];
		mov [context.Eip], eax;
	}

	stackFrame.AddrPC.Offset     = context.Eip;
	stackFrame.AddrPC.Mode       = AddrModeFlat;
	stackFrame.AddrStack.Offset  = context.Esp;
	stackFrame.AddrStack.Mode    = AddrModeFlat;
	stackFrame.AddrFrame.Offset  = context.Ebp;
	stackFrame.AddrFrame.Mode    = AddrModeFlat;

#endif

	while( m_depth < kMaxDepth )
	{
		BOOL result = DbgHelp::StackWalk64( 
			machineType,
			hProcess,
			hThread,
			&stackFrame,
			&context,
			0,
			DbgHelp::SymFunctionTableAccess64,
			DbgHelp::SymGetModuleBase64,
			0 );

		if( !result )
		{
			break;
		}

		m_entries[m_depth] = (size_t)stackFrame.AddrPC.Offset;
		++m_depth;
	}
	DbgHelp::Done();
}
#pragma warning (pop)

void CCPCallstack::LoadSymbols()
{
	DbgHelp::Init();
}

void CCPCallstack::ReleaseSymbols()
{
	DbgHelp::Done();
}

void CCPCallstack::Enumerate( void ( *callback )( size_t codePointer, const char* functionName, void* context ), void* context ) const
{
	DbgHelp::Init();

	HANDLE hProcess = GetCurrentProcess();

	for( unsigned int i = 0; i < m_depth; ++i )
	{
		DWORD64 dwDisplacement;

		ULONG64 buffer[(sizeof(SYMBOL_INFO) +
			MAX_SYM_NAME*sizeof(TCHAR) +
			sizeof(ULONG64) - 1) /
			sizeof(ULONG64)] = { 0 };
		PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;

		pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		pSymbol->MaxNameLen = MAX_SYM_NAME;

		char name[256];

		if( DbgHelp::SymFromAddr( hProcess, m_entries[i], &dwDisplacement, pSymbol) )
		{
			_snprintf_s( name, 256, _TRUNCATE, "%s + 0x%I64X", pSymbol->Name, dwDisplacement );
		}
		else
		{
			name[0] = 0;
		}

		IMAGEHLP_MODULE64 moduleInfo;
		moduleInfo.SizeOfStruct = sizeof( IMAGEHLP_MODULE64 );

		DbgHelp::SymGetModuleInfo64( hProcess, m_entries[i], &moduleInfo );

		callback( m_entries[i], name, context );
	}

	DbgHelp::Done();
}

void CCPCallstack::Enumerate( void ( *callback )( size_t codePointer, void* context ), void* context ) const
{
	DbgHelp::Init();

	for( unsigned int i = 0; i < m_depth; ++i )
	{
		callback( m_entries[i], context );
	}

	DbgHelp::Done();
}

#elif defined(__ANDROID__)

void CCPCallstack::LoadSymbols()
{
}

void CCPCallstack::ReleaseSymbols()
{
}

void CCPCallstack::Capture()
{
}

void CCPCallstack::Enumerate( void ( *callback )( size_t codePointer, void* context ), void* context ) const
{
}

void CCPCallstack::Enumerate( void ( *callback )( size_t codePointer, const char* functionName, void* context ), void* context ) const
{
}

#else

#include <execinfo.h>
#include <cxxabi.h>
#include <string>

void CCPCallstack::LoadSymbols()
{
}

void CCPCallstack::ReleaseSymbols()
{
}

void CCPCallstack::Capture()
{
    m_depth = backtrace( reinterpret_cast<void**>( m_entries ), kMaxDepth );
}

void CCPCallstack::Enumerate( void ( *callback )( size_t codePointer, void* context ), void* context ) const
{
	for( unsigned int i = 0; i < m_depth; ++i )
	{
		callback( m_entries[i], context );
	}
}

namespace
{

void GetLastWord( const char* begin, const char* end, std::string& result )
{
    while( end > begin )
    {
        if( !isspace( *end ) )
        {
            break;
        }
        --end;
    }
    const char* wordStart = end++;
    while( wordStart > begin )
    {
        if( !isalnum( *wordStart ) && *wordStart != '_' )
        {
            break;
        }
        --wordStart;
    }
    result = std::string( ++wordStart, end );
}

void GetFirstWorld( const char* begin, std::string& result )
{
    while( *begin && isspace( *begin ) )
    {
        ++begin;
    }
    if( *begin )
    {
        const char* end = begin;
        while( *end && isalnum( *end ) )
        {
            ++end;
        }
        result = std::string( begin, end );
    }
}
    
void DemangleName( std::string& name )
{
    int status = 0;
    char* buffer = abi::__cxa_demangle( name.c_str(), nullptr, 0, &status );
    if( buffer )
    {
        name = buffer;
        free( buffer );
    }
}
    
}

void CCPCallstack::Enumerate( void ( *callback )( size_t codePointer, const char* functionName, void* context ), void* context ) const
{
    char** lines = backtrace_symbols( reinterpret_cast<void* const *>( m_entries ), m_depth );
    if( !lines )
    {
        return;
    }
    for( size_t i = 0; i < m_depth; ++i )
    {
        std::string record = lines[i];
        const char* plus = strchr( lines[i], '+' );
        if( *plus )
        {
            std::string name;
            GetLastWord( lines[i], plus - 1, name );
            if( !name.empty() )
            {
                DemangleName( name );
                
                std::string offset;
                GetFirstWorld( plus + 1, offset );
                if( !offset.empty() )
                {
                    name += "+";
                    name += offset;
                }
                record = name;
            }
        }
        callback( m_entries[i], record.c_str(), context );
    }
    free( lines );
}


#endif // _MSC_VER

CCPCallstack::CCPCallstack()
: m_depth( 0 )
{
	memset( m_entries, 0, sizeof( m_entries ) );
}

void CCPCallstack::DumpWithSymbols( FILE* fp ) const
{
	Enumerate( &DumpPointerAndNameToFile, fp );
}

void CCPCallstack::DumpRaw( FILE* fp ) const
{
	Enumerate( &DumpPointerToFile, fp );
}

