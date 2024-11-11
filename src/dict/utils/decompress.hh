#pragma once

#include <QByteArray>
#include <string>

/// @param adler32_checksum  0 to skip checksum
QByteArray zlibDecompress( const char * bufptr, unsigned length, unsigned long adler32_checksum );

std::string decompressZlib( const char * bufptr, unsigned length );

std::string decompressBzip2( const char * bufptr, unsigned length );

std::string decompressLzma2( const char * bufptr, unsigned length, bool raw_decoder = false );
