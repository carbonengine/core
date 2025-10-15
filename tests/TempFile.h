// Copyright © 2025 CCP ehf.

#pragma once
#ifndef TempFile_H
#define TempFile_H

#include <string>

class TempFile
{
public:
	TempFile();
	~TempFile();

	FILE* GetFile() const;
	operator FILE*() const;

	std::string GetContents() const;
private:
	FILE* m_stream;
	std::string m_path;
};

#endif