// PluginHelpers.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "Compression.h"

////////////////////////////////////////////////////////////////////////////////////////////////

using namespace System::IO;
using namespace System::IO::Compression; 
using namespace System::Text;

using namespace Abstractspoon::Tdl::PluginHelpers;

////////////////////////////////////////////////////////////////////////////////////////////////
// https://dotnet-snippets.de/snippet/strings-komprimieren-und-dekomprimieren/1058

cli::array<Byte>^ StringCompression::Compress(String^ text)
{
	cli::array<Byte>^ buffer = Encoding::UTF8->GetBytes(text);

	MemoryStream^ memoryStream = gcnew MemoryStream();

	{
		GZipStream^ gZipStream = gcnew GZipStream(memoryStream, CompressionMode::Compress, true);
		gZipStream->Write(buffer, 0, buffer->Length);
	}

	cli::array<Byte>^ compressedData = gcnew cli::array<Byte>((int)memoryStream->Length);

	memoryStream->Position = 0;
	memoryStream->Read(compressedData, 0, compressedData->Length);

	cli::array<Byte>^ gZipBuffer = gcnew cli::array<Byte>(compressedData->Length + 4);

	Buffer::BlockCopy(compressedData, 0, gZipBuffer, 4, compressedData->Length);
	Buffer::BlockCopy(BitConverter::GetBytes(buffer->Length), 0, gZipBuffer, 0, 4); // first dword is length

	return gZipBuffer;
}

String^ StringCompression::Decompress(cli::array<Byte>^ bytes)
{
	MemoryStream^ memoryStream = gcnew MemoryStream();

	int dataLength = BitConverter::ToInt32(bytes, 0); // first dword is length
	memoryStream->Write(bytes, 4, bytes->Length - 4);

	cli::array<Byte>^ buffer = gcnew cli::array<Byte>(dataLength);

	memoryStream->Position = 0;

	{
		GZipStream^ gZipStream = gcnew GZipStream(memoryStream, CompressionMode::Decompress);
		gZipStream->Read(buffer, 0, buffer->Length);
	}

	return Encoding::UTF8->GetString(buffer);
}

////////////////////////////////////////////////////////////////////////////////////////////////

