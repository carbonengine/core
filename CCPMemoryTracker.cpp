// Copyright © 2025 CCP ehf.

// See http://core/wiki/Memory_Tracking

#include "include/CCPMemoryTracker.h"
#include "include/CcpMutex.h"

#include <map>

// #define CCP_UNIT_TEST 1

#include "include/CcpMacros.h"
#include "include/CCPAssert.h"
#include "include/CCPHash.h"
#include "include/CCPCallstack.h"
#include "include/CcpSecureCrt.h"

#ifdef _WIN32
	#include <Psapi.h>
#endif

bool g_isCallstackCaptureEnabled = false;

/// StringTable stores strings, keyed under 32-bit IDs which are calculated as hashes of the strings.
/// Strings are dynamically added and removed from the table, allowing look-up of strings from ID.
/// Strings are reference counted, so the string will be known after multiple Adds until the same
/// number of Remove calls are made.
/// StringTable allows for efficient storage of repeated strings, as the actual string is only
/// stored in memory once and multiple instances of it can be represented by a 32-bit ID.
/// StringTable is initialized with a given block of memory - it is guaranteed not to allocate
/// any memory beyond that.
///
class StringTable
{
public:
	StringTable();
	~StringTable();

	bool Init( void* pMemory, size_t nMemorySize );

	/// Add a string to the table, returning an ID representing the string (a hash of the string).
	unsigned int Add( const char* szInput );

	/// Removes a string from the table with the given ID, as returned from Add.
	/// Note that strings are reference counted, so a string may still exist under the given id
	/// after call to Remove if the string was added multiple times.
	void Remove( unsigned int id );

	/// Retrieve a string with the given ID, as returned from Add.
	const char* LookUp( unsigned int id );

	/// Dump the contents of the string table (for debugging purposes)
	void Dump();
	void* GetMemory( void ) { return m_pMemory; }
	size_t GetMemorySize( void ) { return m_memorySize; }	
	unsigned int GetNumberOfEntries( void ) { return m_numEntries; }

	/// The assumption is that strings will be repeated quite heavily, so most of the time
	/// Add will find that the string being added already exists and the reference count simply
	/// needs to be increased. Similarily Remove will not take the reference down to zero
	/// most of the time. LookUp, on the other hand will not be used frequently - it is only used
	/// by the memory tracker when it's generating reports.
	///
	/// The string table needs to store a look-up table, mapping IDs to the actual string. The strings
	/// need to keep track of their reference count.
	///
	/// In the interest of keeping this class simple, especially in light of the promise not to
	/// dynamically allocate memory, I've chosen to store table entries (ID + pointer to ref-counted
	/// string) in contiguous array, sorted by ID, using binary search to locate entries. When adding
	/// or removing entries, a gap has to be created (or filled) by shifting entries around. This table
	/// is stored at the bottom of the memory block given to the StringTable.
	/// The strings themselves are stored at the top of the memory block, growing downwards. Removing
	/// strings can create gaps, that can be filled by collapsing the string table and patching up the
	/// look table along they, reclaiming unused memory.
	/// If the look-up table and the string table ever meet in the middle, the string table is full.

	struct StringEntry
	{
		size_t m_size;				/// Size of the block
		unsigned int m_refCount;
		char m_string[1];			/// Variable size
	};

	struct TableEntry
	{
		unsigned int m_id;
		StringEntry* m_pString;
	};

private:

	void* m_pMemory;
	size_t m_memorySize;
	unsigned int m_numEntries;
	StringEntry* m_pFirstStringEntry;

	/// Find the given id in the table, or the location where it should go.
	/// Returns true if found, false otherwise. ix is always updated to reflect the proper position
	/// for id in the table.
	bool Find( unsigned int id, unsigned int& ix );

	bool AddString( unsigned int ix, unsigned int id, const char* szString );

	unsigned int AddRefString( unsigned int ix );
	unsigned int ReleaseString( unsigned int ix );

	TableEntry& At( unsigned int ix );
};

StringTable::StringTable()
	: m_pMemory( 0 )
	, m_memorySize( 0 )
	, m_numEntries( 0 )
	, m_pFirstStringEntry( 0 )
{
}

StringTable::~StringTable()
{
}

bool StringTable::Init( void* pMemory, size_t nMemorySize )
{
	CCP_ASSERT( pMemory );
	CCP_ASSERT( nMemorySize > 1024 ); // Need a meaningful size

	m_pMemory = pMemory;
	m_memorySize = nMemorySize;

	/// String entries grow downwards - remove special case of adding first
	/// string entry by initializing string entry pointer to end of memory.
	char* pEnd = (char*)m_pMemory + m_memorySize;
	m_pFirstStringEntry = reinterpret_cast<StringEntry*>( pEnd );

	return true;
}

StringTable::TableEntry& StringTable::At( unsigned int ix )
{
	CCP_ASSERT( m_pMemory );
	CCP_ASSERT( ix < m_numEntries );

	TableEntry* p = static_cast<TableEntry*>( m_pMemory );
	return p[ix];
}

bool StringTable::Find( unsigned int id, unsigned int& ix )
{
	if( m_numEntries == 0 )
	{
		ix = 0;
		return false;
	}

	int lo = 0;
	int hi = m_numEntries;

	while( lo < hi )
	{
		ix = lo + (hi - lo) / 2;
		const TableEntry& te = At( ix );
		if( te.m_id < id )
		{
			hi = ix;
		}
		else if( te.m_id > id )
		{
			++ix;
			lo = ix;
		}
		else
		{
			return true;
		}
	}

	return false;
}

bool StringTable::AddString( unsigned int ix, unsigned int id, const char* szString )
{
	CCP_ASSERT( m_pMemory );

	/// First check to see if we have room for this string
	size_t size = strlen( szString ) + sizeof( TableEntry ) + 1;
	size = (size + 3) & ~3;

	char* pNewStringEntryBottom = (char*)m_pFirstStringEntry;
	pNewStringEntryBottom -= size;

	char* pNewTableTop = (char*)m_pMemory + (m_numEntries + 1) * sizeof( TableEntry );

	if( pNewTableTop >= pNewStringEntryBottom )
	{
		/// TODO: Compact string entries and retry
		return false;
	}


	TableEntry* pEntries = static_cast<TableEntry*>( m_pMemory );
	if( ix < m_numEntries )
	{
		/// Shift entries up to make room for this one
		for( unsigned int destIx = m_numEntries; destIx > ix; --destIx )
		{
			pEntries[destIx] = pEntries[destIx - 1];
		}
	}

	/// String entries grow downwards from top of memory. We keep a pointer to the lowest
	/// one, calculating the size needed for this new one and moving the pointer down.
	/// Strings are not removed when the reference count goes to zero, leaving gaps in
	/// the memory block. The gaps are reclaimed by compacting the memory explicitly.

	/// Fill the StringEntry structure, with initial refcount of 1
	StringEntry* pSE = reinterpret_cast<StringEntry*>( pNewStringEntryBottom );
	pSE->m_size = size;
	pSE->m_refCount = 1;
	char* dst = &pSE->m_string[0];
	strcpy_s( dst, size - sizeof( TableEntry ), szString );

	/// Adjust the pointer for first string entry
	m_pFirstStringEntry = pSE;

	/// Fill the table entry, pointing to the string entry
	TableEntry& te = pEntries[ix];
	te.m_id = id;
	te.m_pString = pSE;

	++m_numEntries;

	return true;
}

unsigned int StringTable::AddRefString( unsigned int ix )
{
	return ++At( ix ).m_pString->m_refCount;
}

unsigned int StringTable::ReleaseString( unsigned int ix )
{
	return --At( ix ).m_pString->m_refCount;
}

unsigned int StringTable::Add( const char* szInput )
{
	if( !szInput )
	{
		return 0;
	}

	CCP_ASSERT( m_pMemory );

	unsigned int id = CcpHashFNV1( szInput, strlen( szInput ) );
	unsigned int ix;

	if( Find( id, ix ) )
	{
		AddRefString( ix );
	}
	else
	{
		bool succeeded = AddString( ix, id, szInput );
		if( !succeeded )
		{
			// Couldn't add string - presumably out of memory
			id = 0;
		}
	}

	return id;
}

void StringTable::Remove( unsigned int id )
{
	CCP_ASSERT( m_pMemory );

	unsigned int ix;

	if( Find( id, ix ) )
	{
		ReleaseString( ix );
	}
}

const char* StringTable::LookUp( unsigned int id )
{
	unsigned int ix;
	if( Find( id, ix ) )
	{
		const TableEntry& te = At( ix );
		return te.m_pString->m_string;
	}
	else
	{
		return 0;
	}
}

void StringTable::Dump()
{
	for( unsigned int i = 0; i < m_numEntries; ++i )
	{
		const TableEntry& te = At( i );

		CCP_LOG( "%s (%d)\n", te.m_pString->m_string, te.m_pString->m_refCount );
	}
}

#if CCP_UNIT_TEST

class StringTableTester
{
public:
	StringTableTester()
	{
		bool succeeded = Run();
		CCP_ASSERT( succeeded );
	}

	bool Run()
	{
		StringTable st;
		st.Init( sStringTableMemory, kStringTableMemorySize );

		char* strings[] = {
			"a",
			"test",
			"Test",
			"This is a little bit longer string",
			";lakdsjf;lkjweroiudvokj;kldfj;lakerjpoewiruoivj;lkdsjf;alkejpoewruoiv;ejf;woeripidfvyuoiejw;rjdf",
			"c:\\ccp\\vistaextreme\\src\\trinity\\CCPMemory.cpp",
			"c:\\ccp\\vistaextreme\\src\\trinity\\CCPAssert.cpp",
			"c:\\ccp\\vistaextreme\\src\\trinity\\TriDevice9.cpp",
			"c:\\ccp\\vistaextreme\\src\\trinity\\bla.cpp",
			"vzxcvkljpeworuoicvuoasidfj;laewkrjk;ljadfkljadsfk;lj ;lasdkfj ;klewjrpoewiur oadsifj ;lkzcxvj klhaesfkhjewiory",
			"wer",
			"werewlkjas;dlfkj;oiuzcv"
		};

		const char* kFirst = "First";
		const char* kSecond = "Second";
		const char* kThird = "Third";

		unsigned int idFirst = st.Add( kFirst );
		const char* szFirst = st.LookUp( idFirst );
		if( !szFirst )
		{
			return false;
		}
		if( strcmp( szFirst, kFirst ) != 0 )
		{
			return false;
		}

		unsigned int idSecond = st.Add( kSecond );
		const char* szSecond = st.LookUp( idSecond );
		if( !szSecond )
		{
			return false;
		}
		if( strcmp( szSecond, kSecond ) != 0 )
		{
			return false;
		}

		szFirst = st.LookUp( idFirst );
		if( !szFirst )
		{
			return false;
		}
		if( strcmp( szFirst, kFirst ) != 0 )
		{
			return false;
		}

		unsigned int idThird = CcpHashFNV1( kThird, strlen( kThird ) );
		const char* szThird = st.LookUp( idThird );
		if( szThird )
		{
			// Shouldn't have found anything
			return false;
		}

		const int kNumStrings = sizeof( strings ) / sizeof( char* );
		unsigned int ids[kNumStrings];

		for( int i = 0; i < 1000; ++i )
		{
			int ix = rand() % kNumStrings;
			ids[ix] = st.Add( strings[ix] );
		}

		for( int idIx = 0; idIx < kNumStrings; ++idIx )
		{
			const char* p = st.LookUp( ids[idIx] );
			if( !p )
			{
				return false;
			}
			if( strcmp( p, strings[idIx] ) != 0 )
			{
				return false;
			}
		}

		st.Dump();

		return true;
	}

private:
	static const int kStringTableMemorySize = 512*1024;
	char sStringTableMemory[kStringTableMemorySize];
};

static StringTableTester sStringTableTester;

#endif // CCP_UNIT_TEST


static class CcpMemoryTracker* s_memoryTracker = nullptr;

#ifdef _WIN32

static HANDLE s_heapForNodeAllocator = INVALID_HANDLE_VALUE;

// Node allocator for use by the MemoryTracker below. This allows it to use stl
// without using regular new operator.
template<class T>
class NodeAllocator : public std::allocator<T>
{
public:
	NodeAllocator()
	{
	}

	NodeAllocator( const NodeAllocator<T>& other )
	{
	}

	template<class OtherType>
	NodeAllocator(const NodeAllocator<OtherType>& other )
	{
	}

	T * allocate(size_t count, const void *hint = 0)
	{
		if( s_heapForNodeAllocator == INVALID_HANDLE_VALUE )
		{
			s_heapForNodeAllocator = HeapCreate( 0, 0, 0 );
		}
		return static_cast<T*>( HeapAlloc( s_heapForNodeAllocator, 0, count * sizeof( T ) ) );
	}

	void deallocate( T* ptr, size_t )
	{
		HeapFree( s_heapForNodeAllocator, 0, ptr );
	}

public:
	template<class OtherType>
	struct rebind
	{	// convert an allocator<T> to an allocator <OtherType>
		typedef NodeAllocator<OtherType> other;
	};
};
#else

template<class T>
class NodeAllocator : public std::allocator<T>
{
public:
	NodeAllocator()
	{
	}
    
	NodeAllocator( const NodeAllocator<T>& other )
	{
	}
    
	template<class OtherType>
	NodeAllocator(const NodeAllocator<OtherType>& other )
	{
	}
    
	T * allocate(size_t count, const void *hint = 0)
	{
		return static_cast<T*>( malloc( count * sizeof( T ) ) );
	}
    
	void deallocate( T* ptr, size_t )
	{
		free( ptr );
	}
    
public:
	template<class OtherType>
	struct rebind
	{	// convert an allocator<T> to an allocator <OtherType>
		typedef NodeAllocator<OtherType> other;
	};
};

#endif

/// MemoryTracker is used to store information about memory allocations. Note that it's important
/// that it does so without ever triggering a call to the memory allocator being tracked, so it's
/// quite limited in what it can use in it's implementation.
///
class CcpMemoryTracker
{
public:

	CcpMemoryTracker();
	~CcpMemoryTracker();
	
	bool Add( void* p, size_t size, const char* pName, const char* pFile, int line );
	bool Remove( void* p );
	bool IsValid( void* p );

	void ReportText( const char* filename );
	void SummaryReport( FILE* file );
	void ReportBinary( const char* filename );

	size_t GetCount() { return m_entryCount; }
	size_t GetSize() { return m_memoryAllocated; }

private:
	// The memory tracker must be thread safe.
	CcpMutex m_mutex;

	// Information about a memory allocation. The strings are stored
	// in a string table so each record holds id's.
	//
	// The records are stored in an stl map, with a custom allocator
	// so its memory allocations don't get mixed in with the tracked
	// allocations we're storing information about here.
	struct MemoryRecord
	{
		void* m_pAddress;
		size_t m_size;
		unsigned int m_moduleNameId;
		unsigned int m_nameId;
		unsigned int m_fileNameId;
		int m_line;
		CCPCallstack m_callstack;
		bool m_isFreed;
	};

	unsigned int m_entryCount;
	size_t m_memoryAllocated;

	static const unsigned int kStringTableMemorySize = 512*1024;
	char m_stringTableMemory[kStringTableMemorySize];
	StringTable m_strings;


	typedef std::map<void*, MemoryRecord, std::less<void*>, NodeAllocator<std::pair<void* const, MemoryRecord> > > MemoryRecordMap;
	MemoryRecordMap m_recordMap;
};

CcpMemoryTracker::CcpMemoryTracker()
	: m_entryCount( 0 )
	, m_memoryAllocated( 0 )
	, m_mutex( "MemoryTracker", "m_mutex" )
{
	m_strings.Init( m_stringTableMemory, kStringTableMemorySize );

	CCPCallstack::LoadSymbols();
}

CcpMemoryTracker::~CcpMemoryTracker()
{
	// TODO: Assert on being empty? Dump report if not empty?

	CCPCallstack::ReleaseSymbols();
}

bool CcpMemoryTracker::Add( void* p, size_t size, const char* pName, const char* pFileName, int line )
{
	CcpAutoMutex guard( m_mutex );

	if( p == 0 )
	{
		return false;
	}

	MemoryRecordMap::iterator it = m_recordMap.find( p );
	if( it != m_recordMap.end() && !it->second.m_isFreed )
	{
		CCP_LOGERR( "Adding a pointer that already exists in memory tracker (%p)", p );
		CCP_LOGERR( "%s (%zd bytes), from %s at %d", pName, size, pFileName, line );
		const char* name = m_strings.LookUp( it->second.m_nameId );
		const char* filename = m_strings.LookUp( it->second.m_fileNameId );
		CCP_LOGERR( "Exists as %s (%zd bytes), from %s at %d", name, it->second.m_size, filename, it->second.m_line );
	}

	MemoryRecord mr;

	mr.m_pAddress = p;
	mr.m_size = size;
	mr.m_moduleNameId = m_strings.Add( "module" );
	mr.m_nameId = m_strings.Add( pName );
	mr.m_fileNameId = m_strings.Add( pFileName );
	mr.m_line = line;
	mr.m_isFreed = false;
	if( g_isCallstackCaptureEnabled )
	{
		mr.m_callstack.Capture();

		// Useful for testing callstack - enable this line, put breakpoint and compare with callstack in debugger
		// mr.m_callstack.DumpWithSymbols();
	}

	m_recordMap[p] = mr;

	++m_entryCount;
	m_memoryAllocated += size;

	return true;
}

bool CcpMemoryTracker::Remove( void* p )
{
	CcpAutoMutex guard( m_mutex );

	if( p == 0 )
	{
		return false;
	}

	MemoryRecordMap::iterator it = m_recordMap.find( p );
	if( it == m_recordMap.end() )
	{
		CCP_LOGWARN( "Freeing a pointer that doesn't exist in memory tracker (%p)", p );
		return false;
	}

	--m_entryCount;
	m_memoryAllocated -= it->second.m_size;

	it->second.m_isFreed = true;

	return true;
}

bool CcpMemoryTracker::IsValid( void* p )
{
	CcpAutoMutex guard( m_mutex );

	MemoryRecordMap::iterator it = m_recordMap.find( p );
	if( it == m_recordMap.end() )
	{
		CCP_LOGERR( "Pointer 0x%p is not valid for the heap", p );
		return false;
	}

	if( it->second.m_isFreed )
	{
		const char* name = m_strings.LookUp( it->second.m_nameId );
		CCP_LOGERR( "Pointer 0x%p has already been freed (%s, %zd)", p, name, it->second.m_size );
		return false;
	}

	return true;
}

static const char* kUnknown = "<Unknown>";

void CcpMemoryTracker::ReportText( const char* filename )
{
	CcpAutoMutex guard( m_mutex );

#ifdef _MSC_VER
	// Turn off tracking for CRT allocations while we're dumping this info
	// to ensure that the m_recordMap iterator below stays valid.
	_CRT_ALLOC_HOOK prev = _CrtSetAllocHook( NULL );
#endif

	CCPCallstack::LoadSymbols();

	FILE* file;
	fopen_s( &file, filename, "w" );
	fprintf( file, "Address, Size, Name, Filename, Line\n" );

	size_t tracked = 0;
	for( MemoryRecordMap::iterator it = m_recordMap.begin(); it != m_recordMap.end(); ++it )
	{
		MemoryRecord& mr = it->second;

		if( mr.m_isFreed )
		{
			continue;
		}

		const char* fileName = m_strings.LookUp( mr.m_fileNameId );
		if( !fileName )
		{
			fileName = kUnknown;
		}
		const char* name = m_strings.LookUp( mr.m_nameId );
		if( !name )
		{
			name = kUnknown;
		}

		tracked += mr.m_size;
		fprintf( file, "0x%p, %" CCP_SIZET_FORMAT ", \"%s\", %s, %d", mr.m_pAddress, mr.m_size, name, fileName, mr.m_line );

		mr.m_callstack.DumpWithSymbols( file );

		fprintf( file, "\n" );
	}
	fprintf( file, "%" CCP_SIZET_FORMAT " bytes allocated total\n\n", CCPMallocUsage() );
	fprintf( file, "%" CCP_SIZET_FORMAT " bytes tracked total\n\n", tracked );

	fclose( file );

	CCPCallstack::ReleaseSymbols();

#ifdef _MSC_VER
	_CrtSetAllocHook( prev );
#endif
}

bool CompareSizeNamePairs( const std::pair<size_t, unsigned int>& p1, const std::pair<size_t, unsigned int>& p2 )
{
	return p1.first < p2.first;
}

void CcpMemoryTracker::SummaryReport( FILE* file )
{
	CcpAutoMutex guard( m_mutex );

	typedef std::map<unsigned int, size_t, std::less<unsigned int>, NodeAllocator<std::pair<const unsigned int, size_t> > > NameSizeMap;
	NameSizeMap sizeByName;
	NameSizeMap countByName;

	for( MemoryRecordMap::iterator it = m_recordMap.begin(); it != m_recordMap.end(); ++it )
	{
		MemoryRecord& mr = it->second;

		if( !mr.m_isFreed )
		{
			sizeByName[mr.m_nameId] += mr.m_size;
			countByName[mr.m_nameId]++;
		}
	}


	typedef std::vector<std::pair<size_t, unsigned int>, NodeAllocator<std::pair<size_t, unsigned int>>> SizeAndNameList;
	SizeAndNameList listForSorting;
	listForSorting.reserve( sizeByName.size() );
	for( auto it = sizeByName.begin(); it != sizeByName.end(); ++it )
	{
		listForSorting.push_back( std::make_pair( it->second, it->first ) );
	}

	std::sort( listForSorting.begin(), listForSorting.end(), CompareSizeNamePairs );

	fprintf( file, "%-50s %12s %6s\n", "Name", "Size", "Count" );

	for( SizeAndNameList::reverse_iterator it = listForSorting.rbegin(); it != listForSorting.rend(); ++it )
	{
		const char* name = m_strings.LookUp( it->second );
		auto count = countByName[it->second];

		fprintf( file, "%-50s %12u %6" CCP_SIZET_FORMAT "\n", name, static_cast<unsigned int>( it->first ), count );
	}
}

void CcpMemoryTracker::ReportBinary( const char* filename )
{
	CcpAutoMutex guard( m_mutex );

#ifdef _MSC_VER
	// Turn off tracking for CRT allocations while we're dumping this info
	// to ensure that the m_recordMap iterator below stays valid.
	_CRT_ALLOC_HOOK prev = _CrtSetAllocHook( NULL );
#endif

	FILE* file;
	fopen_s( &file, filename, "wb" );
	// Number of entries
	size_t mapSize = m_recordMap.size();
	fwrite( &mapSize, sizeof( size_t ), 1, file ); 
	// Number of strings
	unsigned int numStringEntries = m_strings.GetNumberOfEntries();
	fwrite( &numStringEntries, sizeof( unsigned int ), 1, file );
	StringTable::TableEntry *stringData = (StringTable::TableEntry *)m_strings.GetMemory();
	for( unsigned int i = 0; i < numStringEntries; i++ )
	{		
		
		unsigned int id = stringData[i].m_id;
		
		const char *name = stringData[i].m_pString->m_string;
		fwrite( &id, sizeof( unsigned int ), 1, file );

		if ( !name )
		{
			name = kUnknown;
		}		
		size_t len = strlen( name );
		fwrite( &len, sizeof( size_t ), 1, file );
		if ( len > 0 )
		{
			fwrite( name, len, 1, file );
		}
	}
	for( MemoryRecordMap::iterator it = m_recordMap.begin(); it != m_recordMap.end(); ++it )
	{
		MemoryRecord& mr = it->second;
		fwrite( &mr.m_pAddress, sizeof( unsigned int ), 1, file );
		fwrite( &mr.m_size, sizeof( unsigned int ), 1, file );
		fwrite( &mr.m_nameId, sizeof( unsigned int ), 1, file );
		fwrite( &mr.m_moduleNameId, sizeof( unsigned int ), 1, file );
		fwrite( &mr.m_fileNameId, sizeof( unsigned int ), 1, file );
		fwrite( &mr.m_line, sizeof( unsigned int ), 1, file );

		unsigned int numEntries = mr.m_callstack.GetDepth();
		const size_t *entries = mr.m_callstack.GetEntries();
		fwrite( &numEntries, sizeof( unsigned int ), 1, file );
		fwrite( entries, sizeof( size_t ), numEntries, file );
	}
	fclose( file );

#ifdef _MSC_VER
	_CrtSetAllocHook( prev );
#endif
}

// Below are the functions that manage the tracking of information about the memory
// allocations. These functions are called by CCPMallocWithTracking and CCPFreeWithTracking.

void MemoryTrackerInitialize()
{
	if( s_memoryTracker )
	{
		return;
	}

	CCP_ASSERT( s_memoryTracker == NULL );

#ifdef _WIN32
	void* mem = HeapAlloc( GetProcessHeap(), 0, sizeof( CcpMemoryTracker ) );
#else
    void* mem = malloc( sizeof( CcpMemoryTracker ) );
#endif
	CCP_ASSERT( mem != nullptr );

	if( mem )
	{
		s_memoryTracker = new( mem ) CcpMemoryTracker;
	}
}

void MemoryTrackerShutdown()
{
	CCP_ASSERT( s_memoryTracker );

	if( s_memoryTracker )
	{
		s_memoryTracker->~CcpMemoryTracker();
#ifdef _WIN32
		HeapFree( GetProcessHeap(), 0, s_memoryTracker );
#else
        free( s_memoryTracker );
#endif
		s_memoryTracker = nullptr;
	}
}

void MemoryTrackerDumpReportAsText( const char* filename )
{
	CcpMemoryTracker* mt = s_memoryTracker;
	if( mt )
	{
		mt->ReportText( filename );
	}
}

void MemoryTrackerDumpReportAsBinary( const char* filename )
{
	CcpMemoryTracker* mt = s_memoryTracker;
	if( mt )
	{
		mt->ReportBinary( filename );
	}
}

bool IsMemoryTrackingEnabled()
{
	return s_memoryTracker != nullptr;
}

void MemoryTrackerAdd( void* p, size_t size, const char* pName, const char* pFileName, int line )
{
	CcpMemoryTracker* mt = s_memoryTracker;
	if( mt )
	{
		mt->Add( p, size, pName, pFileName, line );
	}
}

void MemoryTrackerRemove( void* p )
{
	CcpMemoryTracker* mt = s_memoryTracker;
	if( mt )
	{
		mt->Remove( p );
	}
}

bool MemoryTrackerIsValid( void* p )
{
	CcpMemoryTracker* mt = s_memoryTracker;
	if( mt )
	{
		return mt->IsValid( p );
	}

	return true;
}

bool MemoryTrackerGetCount( size_t& count )
{
	CcpMemoryTracker* mt = s_memoryTracker;
	if( mt )
	{
		count = mt->GetCount();
		return true;
	}
	else
	{
		count = 0;
		return false;
	}
}

bool MemoryTrackerGetSize( size_t& size )
{
	CcpMemoryTracker* mt = s_memoryTracker;
	if( mt )
	{
		size = mt->GetSize();
		return true;
	}
	else
	{
		size = 0;
		return false;
	}
}

bool IsCallstackCaptureEnabled()
{
	return g_isCallstackCaptureEnabled;
}

void SetCallStackCaptureEnabled( bool enabled )
{
	g_isCallstackCaptureEnabled = enabled;
}

#ifdef _WIN32
size_t GetHeapSizeWithHeapWalk( HANDLE heap )
{
	if( !HeapLock( heap ) )
	{
		return (size_t)-1;
	}
    
	PROCESS_HEAP_ENTRY entry;
	entry.lpData = nullptr;
    
	size_t size = 0;
	while( HeapWalk( heap, &entry ) )
	{
		if( entry.wFlags & PROCESS_HEAP_ENTRY_BUSY )
		{
			size += entry.cbData;
		}
	}
    
	HeapUnlock( heap );
    
	return size;
}

HANDLE MemoryTrackerGetHeapForTracking()
{
	return s_heapForNodeAllocator;
}
#endif

void MemoryTrackerSummaryReportToFile( FILE* file )
{
	if( s_memoryTracker )
	{
		s_memoryTracker->SummaryReport( file );
	}
}
