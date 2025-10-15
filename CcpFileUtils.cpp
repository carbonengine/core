// Copyright © 2014 CCP ehf.


#include "include/CcpFileUtils.h"
#include "include/StringConversions.h"
#include "include/CcpSecureCrt.h"

#include <stdlib.h>

#ifdef _WIN32

int ConvertShareMode( CcpShareMode shareMode )
{
	int shflag = 0;

	switch( shareMode )
	{
	case CCP_SM_NOSHARING:
		shflag = _SH_DENYRW;
		break;

	case CCP_SM_READSHARING:
		shflag = _SH_DENYWR;
		break;

	case CCP_SM_RWSHARING:
		shflag = _SH_DENYNO;
		break;
	}

	return shflag;
}


int ConvertOpenMode( CcpOpenMode mode )
{
	int oflag = 0;

	switch( mode )
	{
	case CCP_OM_READONLY:
		oflag = _O_RDONLY;
		break;

	case CCP_OM_READWRITE:
		oflag = _O_CREAT | _O_RDWR;
		break;
	}

	return oflag;
}

int CcpOpenFile( const wchar_t* filename, CcpOpenMode mode, CcpShareMode shareMode )
{
	int oflag = ConvertOpenMode( mode );
	int shflag = ConvertShareMode( shareMode );

	int fd;
	_wsopen_s( &fd, filename, oflag | _O_BINARY, shflag, _S_IREAD | _S_IWRITE );

	return fd;
}

int CcpCreateFile( const wchar_t* filename, CcpShareMode shareMode )
{
	int fd;

	int shflag = ConvertShareMode( shareMode );
	_wsopen_s( &fd, filename, _O_CREAT | _O_TRUNC | O_RDWR | _O_BINARY, shflag, _S_IREAD | _S_IWRITE );

	return fd;
}

void CcpCloseFile( int fd )
{
	_close( fd );
}

ssize_t CcpReadFromFile( int fd, void* buf, size_t numBytes )
{
	return _read( fd, buf, (unsigned int)numBytes );
}

ssize_t CcpWriteToFile( int fd, const void* buf, size_t numBytes )
{
	return _write( fd, buf, (unsigned int)numBytes );
}

off_t CcpLseek( int fd, off_t offset, int whence )
{
	return _lseek( fd, offset, whence );
}

off_t CcpTell( int fd )
{
	return _tell( fd );
}

#else

int ConvertShareMode( CcpShareMode shareMode )
{
	int shflag = 0;
#ifndef __ANDROID__
	switch( shareMode )
	{
	case CCP_SM_NOSHARING:
		shflag = O_EXLOCK;
		break;

	case CCP_SM_READSHARING:
		shflag = O_SHLOCK;
		break;

	case CCP_SM_RWSHARING:
		shflag = 0;
		break;
	}
#endif
	return shflag;
}

int ConvertOpenMode( CcpOpenMode mode )
{
	int oflag = 0;

	switch( mode )
	{
	case CCP_OM_READONLY:
		oflag = O_RDONLY;
		break;

	case CCP_OM_READWRITE:
		oflag = O_CREAT | O_RDWR;
		break;
	}

	return oflag;
}


int CcpOpenFile( const wchar_t* filename, CcpOpenMode mode, CcpShareMode shareMode )
{
	int oflag = ConvertOpenMode( mode );
	int shflag = ConvertShareMode( shareMode );
	int fd = open( CW2A( filename ), oflag | shflag, S_IRUSR | S_IWUSR );

	return fd;
}

int CcpCreateFile( const wchar_t* filename )
{
	int fd = open( CW2A( filename ), O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR );

	return fd;
}

int CcpCreateFile( const wchar_t* filename, CcpShareMode shareMode )
{
	int shflag = ConvertShareMode( shareMode );
	int fd = open( CW2A( filename ), O_CREAT | O_TRUNC | O_RDWR | shflag, S_IRUSR | S_IWUSR );

	return fd;
}

void CcpCloseFile( int fd )
{
	close( fd );
}

ssize_t CcpReadFromFile( int fd, void* buf, size_t numBytes )
{
	return read( fd, buf, (unsigned int)numBytes );
}

ssize_t CcpWriteToFile( int fd, const void* buf, size_t numBytes )
{
	return write( fd, buf, (unsigned int)numBytes );
}

off_t CcpLseek( int fd, off_t offset, int whence )
{
	return lseek( fd, offset, whence );
}

off_t CcpTell( int fd )
{
	return lseek( fd, 0, SEEK_CUR );
}
#endif

bool CcpRenameFile( const std::wstring& src, const std::wstring& dst )
{
#ifdef _WIN32
	wchar_t srcBuffer[CCP_MAX_PATH];
	wchar_t dstBuffer[CCP_MAX_PATH];
	GetFullPathNameW( src.c_str(), CCP_MAX_PATH, srcBuffer, NULL );
	GetFullPathNameW( dst.c_str(), CCP_MAX_PATH, dstBuffer, NULL );

	if( !MoveFileW( srcBuffer, dstBuffer ) )
	{
		return false;
	}
#else
	if( rename( CW2A( src.c_str() ), CW2A( dst.c_str() ) ) )
	{
		return false;
	}
#endif
	return true;
}

#if defined(_WIN32)
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

#ifdef _WIN32
std::wstring CcpGetAbsolutePath( const std::wstring& name )
{
	std::wstring absName;
	wchar_t buffer[CCP_MAX_PATH];
	DWORD len = GetFullPathNameW( name.c_str(), CCP_MAX_PATH, buffer, NULL );
	if( len > 0 )
	{
		absName = buffer;
		std::replace( absName.begin(), absName.end(), L'\\', L'/' );
	}
	return absName;
}

// Returns true iff name is the path of an existing file.
bool CcpIsPathExistingFile(const std::wstring& name)
{
	DWORD attr = GetFileAttributesW(name.c_str());
	if (attr != INVALID_FILE_ATTRIBUTES)
	{
		return true;
	}

	return false;
}

// Returns true iff name is the path of an existing file.
// adjustedName is the absolute path to the file
bool CcpIsPathExistingFile( const std::wstring& name, std::wstring& absName )
{
	wchar_t buffer[CCP_MAX_PATH];
	DWORD len = GetFullPathNameW( name.c_str(), CCP_MAX_PATH, buffer, NULL );
	if( len > 0 )
	{
		absName = buffer;
		std::replace( absName.begin(), absName.end(), L'\\', L'/' );
		DWORD attr = GetFileAttributesW( buffer );
		if( attr != INVALID_FILE_ATTRIBUTES )
		{
			return true;
		}
	}

	return false;
}

bool CcpIsPathDirectory( const wchar_t* name )
{
	DWORD attr = GetFileAttributesW( name );
	if( attr == INVALID_FILE_ATTRIBUTES )
	{
		return false;
	}
	else
	{
		return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}
}

bool CcpIsPathRelative( const wchar_t* name )
{
    return PathIsRelativeW( name ) != 0;
}

void CcpGetDirectoryContents( const std::wstring& candidate, std::set<std::wstring>& tmpResults )
{
	std::wstring folderWithWildcard = candidate;
	folderWithWildcard += L"\\*";
	WIN32_FIND_DATAW findFileData;
	HANDLE h = FindFirstFileW( folderWithWildcard.c_str(), &findFileData );
	if( h != INVALID_HANDLE_VALUE )
	{
		BOOL isMore = TRUE;
		do
		{
			bool isSpecial = (wcscmp( findFileData.cFileName, L"." ) == 0);
			isSpecial = isSpecial || (wcscmp( findFileData.cFileName, L".." ) == 0);
			if( !isSpecial )
			{
				std::wstring name = findFileData.cFileName;
				for( std::wstring::iterator it = name.begin(); it != name.end(); ++it )
				{
					*it = tolower( *it );
				}
				tmpResults.insert( name );
			}
			isMore = FindNextFileW( h, &findFileData );
		}
		while( isMore );
	}
}

std::wstring CcpGetCurrentWorkingDirectory()
{
	std::wstring result;

	wchar_t cwdBuffer[CCP_MAX_PATH];
	int cdRes = GetCurrentDirectoryW( CCP_MAX_PATH, cwdBuffer );
	if( cdRes > 0 )
	{
		result = cwdBuffer;
		std::replace( result.begin(), result.end(), L'\\', L'/' );
	}
	else
	{
		CCP_LOGERR( "Couldn't get current directory (%d, %d)", cdRes, GetLastError() );
	}
	return result;
}


void CcpRemoveFile( const std::wstring& filename )
{
	BOOL res = DeleteFileW( filename.c_str() );
	if(res == FALSE)
	{
		CCP_LOGERR("Couldn't delete file %S - %d", filename.c_str(), GetLastError());
	}
}

std::wstring CcpExecutablePath()
{
    std::vector<wchar_t> tmp(CCP_MAX_PATH);
    while( GetModuleFileNameW(NULL, &tmp[0], (DWORD)tmp.size()) == tmp.size() )
    {
        tmp.resize(tmp.size()*2);
    }
    return &tmp[0];
}

#else

namespace
{

void AbsPath( const char* path, char* realPath )
{
	char src[PATH_MAX];
	if( path[0] != '/' )
	{
		// If not an absolute path, we need to prepend current
		// working directory
		if( !getcwd( src, sizeof( src ) ) )
		{
			strcpy( realPath, path );
			return;
		}

		auto len = strlen( src );
		if( len > 0 && path[0] != 0 && src[len - 1] != '/' )
		{
			src[len] = '/';
			src[len + 1] = 0;
		}
		strcat( src, path );
	}
	else
	{
		strcpy( src, path );
	}

	// Collect leading slashes: there can be at most 2 of them.
	// All the others should be eaten up
	auto start = src;
	size_t initialSlashes = 0;
	while( *start == '/' )
	{
		++initialSlashes;
		++start;
		if( initialSlashes == 2 )
		{
			break;
		}
	}

	// Split the path by slashes
	auto end = start;
	std::vector<std::pair<char*, char*>> comps;
	while( true )
	{
		if( *end == '/' || *end == 0 )
		{
			// Empty comopnents (like in //) and . are ignored
			if( start != end && ( end - start != 1 || *start != '.' ) )
			{
				// If the component is .., pop the previous component
				if( end - start == 2 && start[0] == '.' && start[1] == '.' )
				{
					if( !comps.empty() )
					{
						comps.pop_back();
					}
				}
				else
				{
					comps.push_back( std::make_pair( start, end ) );
				}
			}
			if( *end == 0 )
			{
				break;
			}
			else
			{
				start = end + 1;
			}
		}
		++end;
	}

	// Put initial slashes back in
	end = realPath;
	for( size_t i = 0; i < initialSlashes; ++i )
	{
		*end = '/';
		++end;
	}

	// Join path components
	for( size_t i = 0; i < comps.size(); ++i )
	{
		if( i )
		{
			*end = '/';
			++end;
		}
		auto len = comps[i].second - comps[i].first;
		strncpy( end, comps[i].first, len );
		end += len;
	}
	*end = 0;
}

}

std::wstring CcpGetAbsolutePath( const std::wstring& name )
{
	if( name.empty() )
	{
		return L"";
	}
	auto nameA = CW2A( name.c_str() );
	char buffer[PATH_MAX];
	strcpy_s( buffer, nameA );
	AbsPath( CW2A( name.c_str() ), buffer );
    std::wstring absName = std::wstring( CA2W( buffer ) );
    if( CcpIsPathDirectory( absName.c_str() ) && !absName.empty() && absName[absName.length() - 1] != L'/' )
    {
        absName += L"/";
    }
    return absName;
}

void BreakPathIntoComponents( const std::string& path, std::vector<std::string>& components )
{
	size_t endPos = 0;
	size_t startPos = path.find_first_of( '/', endPos );
	if( startPos == std::string::npos )
	{
		components.push_back( path );
		return;
	}
	while( startPos != std::string::npos )
	{
		++startPos;
		endPos = path.find_first_of( '/', startPos );
		size_t len;
		if( endPos == std::string::npos )
		{
			len = endPos;
		}
		else
		{
			len = endPos - startPos;
		}
		std::string comp = path.substr( startPos, len );

		components.push_back( comp );
		startPos = endPos;
	} 
}

// Removes any '.' entries, and collapses '..' entries with the entry preceding it.
// For example, '/root/a/b/../.././c' becomes '/root/c'.
void CollapseRelativePaths( std::vector<std::string>& components )
{
	for( auto it = components.begin(); it != components.end(); )
	{
		if( *it == "." ) 
		{
			it = components.erase( it );
		}
		else
		{
			auto next = it + 1;
			if( next != components.end() )
			{
				if( *next == ".." )
				{
					components.erase( next );
					components.erase( it );
					it = components.begin();
				}
				else
				{
					++it;
				}
			}
			else
			{
				++it;
			}
		}
	}
}

void GetPossibleCasings( const std::string& path, const std::string& nextComponent, std::vector<std::string>& possibleCasings )
{
	DIR* dpdf = opendir( path.c_str() );

	if( dpdf )
	{
		struct dirent entry;
		struct dirent* result;
		int resultCode = readdir_r( dpdf, &entry, &result );
		while( (resultCode == 0) && result )
		{
			if( strcasecmp( nextComponent.c_str(), result->d_name ) == 0 )
			{
				possibleCasings.push_back( result->d_name );
			}
			resultCode = readdir_r( dpdf, &entry, &result );
		}

		closedir( dpdf );
	}
}

// Return value indicates whether search should continue or not.
// If true, continue searching.
// If false, searching is over. In this case, the result string contains the string if found, otherwise it is empty
// meaning the search was exhausted without finding a match.
bool FindCasingCorrectedPath( const std::string& path, const std::vector<std::string>& components, size_t compIx, std::string& result )
{
	std::vector<std::string> possibleCasings;
	GetPossibleCasings( path, components[compIx], possibleCasings );

	for( auto it = possibleCasings.begin(); it != possibleCasings.end(); ++it )
	{
		std::string candidate = path;
		if( compIx > 0 )
		{
			candidate += '/';
		}
		candidate += *it;

		if( compIx == components.size() - 1 )
		{
			// We're at a leaf node
			struct stat statData;
			int statResult = stat( candidate.c_str(), &statData );

			if( statResult == 0 )
			{
				result = candidate;
				return false;
			}
			else
			{
				return true;
			}
		}
		else		
		{
			bool isMore = FindCasingCorrectedPath( candidate, components, compIx + 1, result );
			if( !isMore )
			{
				return false;
			}
		}
	}

	// Returning true here to keep the search going - we want to force the search to all leaf nodes
	return true;
}

bool CcpIsPathExistingFile( const std::wstring& name, std::wstring& adjustedName )
{
	std::string nameA = (const char*)CW2A( name.c_str() );

	struct stat statData;
	int statResult = stat( nameA.c_str(), &statData );

	if( statResult != 0 )
	{
		// Path not found - try to correct casing
		std::vector<std::string> components;
		BreakPathIntoComponents( nameA, components );
		CollapseRelativePaths( components );

		std::string root = "/";
		std::string adjustedNameA;
		FindCasingCorrectedPath( root, components, 0, adjustedNameA );
		if( !adjustedNameA.empty() )
		{
			adjustedName = CA2W( adjustedNameA.c_str() );
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
        adjustedName = name;
		return true;
	}
}

bool CcpIsPathExistingFile( const std::wstring& name )
{
	std::wstring adjustedName;
	return CcpIsPathExistingFile(name, adjustedName);
}

bool CcpIsPathDirectory( const wchar_t* name )
{
	std::string nameA = (const char*)CW2A( name );
	struct stat statData;
	int statResult = stat( nameA.c_str(), &statData );

	if( statResult == 0 )
	{
		return S_ISDIR( statData.st_mode );
	}
	else
	{
		return false;
	}
}

bool CcpIsPathRelative( const wchar_t* name )
{
    return name && name[0] != L'/';
}

void CcpGetDirectoryContents( const std::wstring& candidate, std::set<std::wstring>& tmpResults )
{
	std::string folderName = (const char*)CW2A( candidate.c_str() );
	DIR* dpdf = opendir( folderName.c_str() );

	if( dpdf )
	{
		struct dirent entry;
		struct dirent* result;
		int resultCode = readdir_r( dpdf, &entry, &result );
		while( (resultCode == 0) && result )
		{
			bool isSpecial = (strcmp( result->d_name, "." ) == 0);
			isSpecial = isSpecial || (strcmp( result->d_name, ".." ) == 0);
			if( !isSpecial )
			{
				std::wstring name = (const wchar_t*)CA2W( result->d_name );
				for( std::wstring::iterator it = name.begin(); it != name.end(); ++it )
				{
					*it = tolower( *it );
				}
				tmpResults.insert( name );
			}
			resultCode = readdir_r( dpdf, &entry, &result );
		}

		closedir( dpdf );
	}
}

std::wstring CcpGetCurrentWorkingDirectory()
{
	std::wstring result;

	static const int BUFFER_SIZE = 1024;
	char cwdBuffer[BUFFER_SIZE];
	if( getcwd( cwdBuffer, BUFFER_SIZE ) )
	{
		result = (const wchar_t*)CA2W( cwdBuffer );
	}
	else
	{
		CCP_LOGERR( "Couldn't get current directory (%d)", errno );
	}

	return result;
}


void CcpRemoveFile( const std::wstring& filename )
{
	std::string name = (const char*)CW2A( filename.c_str() );
	remove( name.c_str() );
}

std::wstring CcpExecutablePath()
{
#ifdef __APPLE__
    std::vector<char> tmp(CCP_MAX_PATH);
    uint32_t size = uint32_t( tmp.size() );
    if( _NSGetExecutablePath( &tmp[0], &size ) < 0 )
    {
        tmp.resize( size );
        _NSGetExecutablePath( &tmp[0], &size );
    }
    
    char actualpath [PATH_MAX];
    char* path = realpath(&tmp[0], actualpath);
    return std::wstring( CA2W( actualpath ) );
#else
    static_assert( false, "CcpExecutablePath is not implemented" );
#endif
}


#endif
