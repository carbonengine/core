// Copyright © 2025 CCP ehf.

#ifndef _CCPCALLSTACK_H_
#define _CCPCALLSTACK_H_

#include "carbon_core_export.h"

class CARBON_CORE_API CCPCallstack
{
public:
	CCPCallstack();
	void Capture();

	/// Ensures symbols are loaded for more efficient handling of multiple calls
	/// to DumpWithSymbols.
	static void LoadSymbols();
	static void ReleaseSymbols();

	/// Dumps the callstack, with symbols where possible, to the debugger console.
	/// Note that when dumping multiple callstacks, it is far more efficient to
	/// call LoadSymbols before the first DumpWithSymbols call - otherwise symbols
	/// are loaded and released for each call.
	void DumpWithSymbols( FILE* fp ) const;

	//// Dumps the callstack as raw hex numbers, comma separated.
	void DumpRaw( FILE* fp ) const;

	template <typename Callback>
	void EnumerateAddresses( Callback cb ) const
	{
		Enumerate( &EnumerateAddressCallback<Callback>, &cb );
	}

	template <typename Callback>
	void EnumerateSymbols( Callback cb ) const
	{
		Enumerate( &EnumerateSymbolsCallback<Callback>, &cb );
	}
	
	unsigned int GetDepth( void ) const { return m_depth; }
	const size_t * GetEntries( void ) const { return (const size_t*)&m_entries; }
private:
	template <typename Callback>
	static void EnumerateAddressCallback( size_t codePointer, void* context )
	{
		( *static_cast<Callback*>( context ) )( codePointer );
	}
	template <typename Callback>
	static void EnumerateSymbolsCallback( size_t codePointer, const char* name, void* context )
	{
		( *static_cast<Callback*>( context ) )( codePointer, name );
	}

	void Enumerate( void ( *callback )( size_t codePointer, void* context ), void* context ) const;
	void Enumerate( void ( *callback )( size_t codePointer, const char* functionName, void* context ), void* context ) const;

	static const int kMaxDepth = 32;

	unsigned int m_depth;
	size_t m_entries[kMaxDepth];
};

#endif

