/*
The MIT License (MIT)

Copyright (c) 2015 Logicore Software

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/*
To build (with the compiler provided with Microsoft Windows SDK):
cd "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\x86_amd64"
vcvarsx86_amd64.bat
cd <code path>
cl <code path>\duplicates.cpp <code path>\PMurHash.c /O2 /EHsc /GA /MT /Fe<code path>\Dedup.exe
*/


#include <stdio.h>
#include <windows.h>
#include <vector>
#include <map>
#include <list>
#include <stdint.h>
#include "PMurHash.h"

#define BUFFERSIZE (64*1024*1024)

using namespace std;

class oneFile
{
	public:
	wstring path;
	wstring name;
	uint64_t timeStamp;
};

map<uint64_t,map<uint32_t,list<oneFile>>> g_files;	//Map of sizes,map of hashes, list of objects
void RecurseFilePath(wstring path);
uint32_t CalculateFileHash(const wstring& path, WIN32_FIND_DATAW& findData,uint64_t fileSize);
void checkDuplicates(bool deleteFiles,bool showDuplicates);
bool AreDuplicates(const wstring& file1, const wstring& file2, uint64_t fileSize);
vector<BYTE> g_buffer;
vector<BYTE> g_buffer2;
uint64_t g_filesProcessed=0;


int wmain(int argc, wchar_t* argv[])
{
	bool deleteFiles=false;
	bool showDuplicates=false;
	printf("Dedup (c) 2015 Logicore Software\n");
	printf("www.logicore.se\n");
	printf("The software is provided as is. Use at your own risk.\n");
	if(argc<2)
	{
		printf("Usage: Dedup <folder> <options>\n");
		printf("This program will find duplicate files within a folder\n");
		printf("Options:\n");
		printf("-D - delete duplicates, keeping the file with the oldest creation date\n");
		printf("-S - show each duplicate\n");
		return 0;
	}
	if(argc>=3)
	{
		for(int i=2;i<argc;i++)
		{
			if(!wcscmp(argv[i],L"-D"))
				deleteFiles=true;
			if(!wcscmp(argv[i],L"-S"))
				showDuplicates=true;
		}	
	}		
	g_buffer.resize(BUFFERSIZE);
	g_buffer2.resize(BUFFERSIZE);

	wstring path=argv[1];
	printf("Hashing files...\n");
	RecurseFilePath(path);
	printf("%I64d Files hashed. Performing comparisons\n",g_filesProcessed);
	checkDuplicates(deleteFiles,showDuplicates);
	return 0;
}

void checkDuplicates(bool deleteFiles,bool showDuplicates)
{
	uint64_t duplicates=0;
	uint64_t bytesSaved=0;
	
	for(auto& o : g_files)	//Loop over file sizes
	{
		for(auto& o2 : o.second)		//loop over hash values
		{
			bool stillOK=true;
			if(o2.second.size()>1 && stillOK)
			{
				stillOK=false;
				oneFile of=o2.second.front();
				// Figure out which file is oldest.
				for each(auto& o3 in o2.second)
				{
					if(o3.timeStamp<of.timeStamp)
						of=o3;
				}
				wstring fileName1=of.path+L"\\"+of.name;
				for(list<oneFile>::iterator o3=o2.second.begin();o3!=o2.second.end();)
				{
					if(!( (o3->path== of.path) && (o3->name== of.name) ))
					{
						wstring fileName2=o3->path+L"\\"+o3->name;
						if(AreDuplicates(fileName1, fileName2,o.first))
						{
							if(showDuplicates)
								wprintf(L"%s is a duplicate of %s\n",fileName2.c_str(),fileName1.c_str());
							if(deleteFiles)
								DeleteFileW(fileName2.c_str());
							stillOK=true;
							o3=o2.second.erase(o3);
							duplicates++;
							bytesSaved+=o.first;
							continue;	//to the loop
						}
					}
					else
					{
						stillOK=true;
						o3=o2.second.erase(o3);
						continue;
					}
					o3++;
				}
			}
		}
	}
	printf("%I64d duplicates found corresponding to %I64d kilobytes.\n",duplicates,bytesSaved/1024);
}

void RecurseFilePath(wstring path)
{
	WIN32_FIND_DATAW findData;
	wstring pathWithWildCard=path+L"\\*";
	bool first=true;
	HANDLE h=0;
	
	wprintf(L"%s\n",path.c_str());

	do
	{
		if(first)
		{
			h=FindFirstFileW(pathWithWildCard.c_str(),&findData);
			if(INVALID_HANDLE_VALUE==h)
			{
				printf("Error %d\n",GetLastError());
				return;
			}
			first=false;
		}
		else
		{
			if(!FindNextFileW(h,&findData))
			{
				FindClose(h);
				return;
			}
		}
		
		if(findData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
		{
			if( wcscmp(findData.cFileName,L".") && wcscmp(findData.cFileName,L"..") )
			{
				wstring newPath=path+L"\\";
				newPath+=findData.cFileName;
				RecurseFilePath(newPath);
			}
		}
		else
		{
			oneFile o;
			uint64_t fileSize=findData.nFileSizeHigh;
			fileSize=fileSize<<32;
			fileSize+=findData.nFileSizeLow;
			uint32_t hash=CalculateFileHash(path,findData,fileSize);
			

			o.timeStamp=findData.ftCreationTime.dwHighDateTime;
			o.timeStamp=o.timeStamp<<32;
			o.timeStamp+=findData.ftCreationTime.dwLowDateTime;

			o.name=findData.cFileName;
			o.path=path;
			g_files[fileSize][hash].push_back(o);
			g_filesProcessed++;
		}
	}while(true);	
}
bool AreDuplicates(const wstring& file1, const wstring& file2, uint64_t fileSize)
{
	FILE* fp1=_wfopen(file1.c_str(),L"rb");
	if(fp1)
	{
		FILE* fp2=_wfopen(file2.c_str(),L"rb");
		if(fp2)
		{
			while(fileSize)
			{
				__int64 thisRead;
				
				if(fileSize>BUFFERSIZE)
					thisRead=BUFFERSIZE;
				else
					thisRead=fileSize;
				
				fileSize-=thisRead;
				
				fread(&g_buffer[0],1,thisRead,fp1);
				fread(&g_buffer2[0],1,thisRead,fp2);
				
				for(int i=0;i<thisRead;i++)
				{
					if(g_buffer[i]!=g_buffer2[i])
					{
						fclose(fp1);
						fclose(fp2);
						return false;
					}
				}
			}
			fclose(fp2);
		}
		fclose(fp1);
	}
	return true;
}

uint32_t CalculateFileHash(const wstring& path, WIN32_FIND_DATAW& findData,uint64_t fileSize)
{
	MH_UINT32 hash=0;
	MH_UINT32 carry=0;
	uint64_t totalSize=fileSize;
	
	wstring newPath=path+L"\\";
	newPath+=findData.cFileName;
	FILE* fp=_wfopen(newPath.c_str(),L"rb");
	if(fp)
	{
		while(fileSize)
		{
			__int64 thisRead;
			
			if(fileSize>BUFFERSIZE)
				thisRead=BUFFERSIZE;
			else
				thisRead=fileSize;
			
			fileSize-=thisRead;
			
			fread(&g_buffer[0],1,thisRead,fp);
			
			PMurHash32_Process(&hash,&carry,&g_buffer[0],thisRead);
		}
		fclose(fp);
		return (uint32_t)PMurHash32_Result(hash,carry,totalSize);
	}
	return 0;
}