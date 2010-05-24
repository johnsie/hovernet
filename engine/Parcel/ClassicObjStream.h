
// ClassicObjStream.h
// Standard HoverRace 1.x parcel data stream.
//
// Copyright (c) 2010 Michael Imamura.
//
// Licensed under GrokkSoft HoverRace SourceCode License v1.0(the "License");
// you may not use this file except in compliance with the License.
//
// A copy of the license should have been attached to the package from which
// you have taken this file. If you can not find the license you can not use
// this file.
//
//
// The author makes no representations about the suitability of
// this software for any purpose.  It is provided "as is" "AS IS",
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied.
//
// See the License for the specific language governing permissions
// and limitations under the License.

#pragma once

#include "ObjStream.h"

#ifdef _WIN32
#	ifdef MR_ENGINE
#		define MR_DllDeclare   __declspec( dllexport )
#	else
#		define MR_DllDeclare   __declspec( dllimport )
#	endif
#else
#	define MR_DllDeclare
#endif

namespace HoverRace {
namespace Parcel {

/**
 * Standard HoverRace 1.x parcel data stream.
 * @author Michael Imamura
 * @todo Handle big-endian platforms.
 */
class MR_DllDeclare ClassicObjStream : public ObjStream
{
	typedef ObjStream SUPER;
	public:
		ClassicObjStream(std::iostream &stream, const std::string &name, bool writing);
		virtual ~ClassicObjStream() { }

		virtual void Write(const void *buf, size_t ct) { stream->write((const char*)buf, ct); }

		virtual void WriteUInt8(MR_UInt8 i);
		virtual void WriteInt16(MR_Int16 i);
		virtual void WriteUInt16(MR_UInt16 i);
		virtual void WriteInt32(MR_Int32 i);
		virtual void WriteUInt32(MR_UInt32 i);
		virtual void WriteString(const std::string &s);
#		ifdef _WIN32
			virtual void WriteCString(const CString &s) { WriteString((const char *)s); }
#		endif

		virtual void Read(void *buf, size_t ct) { stream->read((char*)buf, ct); }

		virtual void ReadUInt8(MR_UInt8 &i);
		virtual void ReadInt16(MR_Int16 &i);
		virtual void ReadUInt16(MR_UInt16 &i);
		virtual void ReadInt32(MR_Int32 &i);
		virtual void ReadUInt32(MR_UInt32 &i);
		virtual void ReadString(std::string &s);
#		ifdef _WIN32
			virtual void ReadCString(CString &s) { std::string ss; ReadString(ss); s = ss.c_str(); }
#		endif

	private:
		MR_UInt32 ReadStringLength();

	private:
		std::iostream *stream;
};

}  // namespace Parcel
}  // namespace HoverRace

#undef MR_DllDeclare