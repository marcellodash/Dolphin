// Copyright (C) 2010 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "CommonPaths.h"
#include "FileUtil.h"
#include "NandPaths.h"
#include <fstream>

namespace Common
{

std::string GetTicketFileName(u64 _titleID)
{
	char TicketFilename[1024];
	sprintf(TicketFilename, "%sticket/%08x/%08x.tik",
			File::GetUserPath(D_WIIUSER_IDX).c_str(), (u32)(_titleID >> 32), (u32)_titleID);

	return TicketFilename;
}

std::string GetTitleDataPath(u64 _titleID)
{
	char path[1024];
	sprintf(path, "%stitle/%08x/%08x/data/",
			File::GetUserPath(D_WIIUSER_IDX).c_str(), (u32)(_titleID >> 32), (u32)_titleID);

	return path;
}

std::string GetTMDFileName(u64 _titleID)
{
	return GetTitleContentPath(_titleID) + "title.tmd";
}
std::string GetTitleContentPath(u64 _titleID)
{
	char ContentPath[1024];
	sprintf(ContentPath, "%stitle/%08x/%08x/content/",
			File::GetUserPath(D_WIIUSER_IDX).c_str(), (u32)(_titleID >> 32), (u32)_titleID);

	return ContentPath;
}

bool CheckTitleTMD(u64 _titleID)
{
	const std::string TitlePath = GetTMDFileName(_titleID);
	if (File::Exists(TitlePath))
	{
		File::IOFile pTMDFile(TitlePath, "rb");
		u64 TitleID = 0;
		pTMDFile.Seek(0x18C, SEEK_SET);
		if (pTMDFile.ReadArray(&TitleID, 1) && _titleID == Common::swap64(TitleID))
			return true;
	}
	INFO_LOG(DISCIO, "Invalid or no tmd for title %08x %08x", (u32)(_titleID >> 32), (u32)(_titleID & 0xFFFFFFFF));
	return false;
}

bool CheckTitleTIK(u64 _titleID)
{
	const std::string ticketFileName = Common::GetTicketFileName(_titleID);	
	if (File::Exists(ticketFileName))
	{
		File::IOFile pTIKFile(ticketFileName, "rb");
		u64 TitleID = 0;
		pTIKFile.Seek(0x1dC, SEEK_SET);
		if (pTIKFile.ReadArray(&TitleID, 1) && _titleID == Common::swap64(TitleID))
			return true;
	}
	INFO_LOG(DISCIO, "Invalid or no tik for title %08x %08x", (u32)(_titleID >> 32), (u32)(_titleID & 0xFFFFFFFF));
	return false;
}

static void CreateReplacementFile(std::string &filename)
{
	std::ofstream replace(filename.c_str());
	replace <<"\" __22__\n";
	replace << "* __2a__\n";
	//replace << "/ __2f__\n";
	replace << ": __3a__\n";
	replace << "< __3c__\n";
	replace << "> __3e__\n";
	replace << "? __3f__\n";
	//replace <<"\\ __5c__\n";
	replace << "| __7c__\n";
}

void ReadReplacements(replace_v& replacements)
{
	replacements.clear();
	const std::string replace_fname = "/sys/replace";
	std::string filename(File::GetUserPath(D_WIIROOT_IDX));
	filename += replace_fname;

	if (!File::Exists(filename))
		CreateReplacementFile(filename);

	std::ifstream f(filename.c_str());
	char letter;
	std::string replacement;

	while (f >> letter >> replacement && replacement.size())
		replacements.push_back(std::make_pair(letter, replacement));
}

};
