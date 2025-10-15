// Copyright © 2025 CCP ehf.

#include "TempFile.h"
#include "CcpCore.h"
#include <memory>


TempFile::TempFile()
	:m_stream( nullptr )
{
#ifdef _WIN32
	char buffer[MAX_PATH];
	auto len = GetTempPath(MAX_PATH, buffer );
    if( len > MAX_PATH || len == 0 )
    {
		return;
    }

	char path[MAX_PATH];
    auto result = GetTempFileName( buffer, "ccpcoretest", 0, path );
    if( !result )
    {
		return;
    }
	m_path = path;
	fopen_s( &m_stream, path, "w+t" );
#else
    m_stream = tmpfile();
#endif
}

TempFile::~TempFile()
{
	if( m_stream )
	{
		fclose( m_stream );
	}
#ifdef _WIN32
	if( !m_path.empty() )
	{
		DeleteFile( m_path.c_str() );
	}
#endif
}

FILE* TempFile::GetFile() const
{
	return m_stream;
}

TempFile::operator FILE*() const
{
	return m_stream;
}

std::string TempFile::GetContents() const
{
	std::string result;
	if( m_stream )
	{
		size_t length = ftell( m_stream );
		if( length )
		{
			std::unique_ptr<char[]> buffer( new char[length] );
			fseek( m_stream, 0, SEEK_SET );
			fread( buffer.get(), 1, length, m_stream );
			result = std::string( buffer.get(), buffer.get() + length );
		}
	}
	return result;
}
