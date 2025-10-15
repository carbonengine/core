// Copyright © 2014 CCP ehf.

#pragma once
#ifndef CcpFileUtils_h
#define CcpFileUtils_h

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <share.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#include <set>
#include <string>

#include "carbon_core_export.h"

#ifdef  _WIN64
typedef __int64    ssize_t;
#elif defined(_WIN32)
typedef _W64 int   ssize_t;
#endif

#define CCP_MAX_PATH 4096

enum CcpOpenMode
{
	CCP_OM_READWRITE,
	CCP_OM_READONLY
};

enum CcpShareMode
{
	CCP_SM_NOSHARING,
	CCP_SM_READSHARING,
	CCP_SM_RWSHARING
};

CARBON_CORE_API bool CcpRenameFile( const std::wstring& src, const std::wstring& dst );
CARBON_CORE_API std::wstring CcpGetAbsolutePath( const std::wstring& name );
CARBON_CORE_API bool CcpIsPathExistingFile(const std::wstring& name);
CARBON_CORE_API bool CcpIsPathExistingFile(const std::wstring& name, std::wstring& absName);
CARBON_CORE_API bool CcpIsPathDirectory( const wchar_t* name );
CARBON_CORE_API bool CcpIsPathRelative( const wchar_t* name );
CARBON_CORE_API void CcpGetDirectoryContents( const std::wstring& directory, std::set<std::wstring> &results );
CARBON_CORE_API std::wstring CcpGetCurrentWorkingDirectory();
CARBON_CORE_API void CcpRemoveFile( const std::wstring& filename );
CARBON_CORE_API std::wstring CcpExecutablePath();
CARBON_CORE_API int CcpOpenFile( const wchar_t* filename, CcpOpenMode mode, CcpShareMode shareMode );
CARBON_CORE_API int CcpCreateFile( const wchar_t* filename, CcpShareMode shareMode );
CARBON_CORE_API void CcpCloseFile( int fd );
CARBON_CORE_API ssize_t CcpReadFromFile( int fd, void* buf, size_t numBytes );
CARBON_CORE_API ssize_t CcpWriteToFile( int fd, const void* buf, size_t numBytes );
CARBON_CORE_API off_t CcpLseek( int fd, off_t offset, int whence );
CARBON_CORE_API off_t CcpTell( int fd );

#endif // CcpFileUtils_h