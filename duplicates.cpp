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

#include <stdio.h>
#include <windows.h>
#include <vector>
#include <map>
#include <list>
#include <stdint.h>
#include "PMurHash.h"

#define BUFFERSIZE (64*1024*1024)
#define VERSION "0.3"

using namespace std;

class oneFile
{
	public:
	wstring path;
	wstring name;
	uint64_t timeStamp;
};

map<uint64_t,list<oneFile>> g_files;	//Map of sizes,map of hashes, list of objects
void RecurseFilePath(wstring path);
uint32_t CalculateFileHash(const wstring& newPath, uint64_t fileSize);
void checkDuplicates(bool deleteFiles,bool showDuplicates,bool linkFiles);
bool AreDuplicates(const wstring& file1, const wstring& file2, uint64_t fileSize);
vector<BYTE> g_buffer;
vector<BYTE> g_buffer2;
uint64_t g_filesProcessed=0;


int wmain(int argc, wchar_t* argv[])
{
	DWORD timeTaken=GetTickCount();
	bool deleteFiles=false;
	bool showDuplicates=false;
	bool linkFiles=false;
	printf("Dedup v%s (c) 2015 Logicore Software\n",VERSION);
	printf("www.logicore.se\n");
	printf("The software is provided as is. Use at your own risk.\n");
	if(argc<2)
	{
		printf("Usage: Dedup <folder> <options>\n");
		printf("This program will find duplicate files within a folder\n");
		printf("Options:\n");
		printf("-D - delete duplicates, keeping the file with the oldest creation date\n");
		printf("-L - create links for duplicates, keeping the file with the oldest creation date\n");
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
			if(!wcscmp(argv[i],L"-L"))
				linkFiles=true;

		}	
	}		
	
	if(linkFiles && deleteFiles)
	{
		printf("You cannot both link and delete files!\n");
		return 0;
	}
	g_buffer.resize(BUFFERSIZE);
	g_buffer2.resize(BUFFERSIZE);

	wstring path=argv[1];
	printf("Scanning files...\n");
	RecurseFilePath(path);
	printf("%I64d Files found. Performing comparisons\n",g_filesProcessed);
	checkDuplicates(deleteFiles,showDuplicates,linkFiles);
	
	timeTaken=GetTickCount()-timeTaken;
	printf("Time taken: %d seconds\n",timeTaken/1000);
	return 0;
}

void checkDuplicates(bool deleteFiles,bool showDuplicates,bool linkFiles)
{
	uint64_t duplicates=0;
	uint64_t bytesSaved=0;
	
	for(auto& o : g_files)	//Loop over file sizes
	{
		if(o.second.size()>1)	//Size collision, need to hash & compare
		{
			map<uint32_t,list<oneFile>> hashes;
			for(auto& o2 : o.second)		//loop over hash values
			{
				wstring fileName=o2.path+L"\\"+o2.name;
				uint32_t hash=CalculateFileHash(fileName,o.first);
				hashes[hash].push_back(o2);
			}
			
			for(auto& o2 : hashes)		//loop over hash values
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
								{
									if(!DeleteFileW(fileName2.c_str()));
										wprintf(L"Could not delete %s\n",fileName2.c_str());
								}
								else if(linkFiles)
								{
									wchar_t fname[MAX_PATH];
									//Not really pleased with the MAX_PATH limitation here...
									UINT res=GetTempFileNameW(o3->path.c_str(),L"DED",0,fname);	//We will use a temp file first in case CreateHardLink fails
									if(res)
									{
										DeleteFileW(fname);		//Delete the new empty file, we just wanted a file name to use in CreateHardLink
										if(CreateHardLinkW(fname,fileName1.c_str(),NULL))
										{
											DeleteFileW(fileName2.c_str());		//Delete the original duplicate
											_wrename(fname,fileName2.c_str());	//rename temp file to original duplicate
										}
										else
											wprintf(L"Could not link %s to %s\n",fileName2.c_str(),fileName1.c_str());
									}
									else
										wprintf(L"Could not link %s to %s\n",fileName2.c_str(),fileName1.c_str());
								}
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

			o.timeStamp=findData.ftCreationTime.dwHighDateTime;
			o.timeStamp=o.timeStamp<<32;
			o.timeStamp+=findData.ftCreationTime.dwLowDateTime;

			o.name=findData.cFileName;
			o.path=path;
			g_files[fileSize].push_back(o);
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

uint32_t CalculateFileHash(const wstring& newPath, uint64_t fileSize)
{
	MH_UINT32 hash=0;
	MH_UINT32 carry=0;
	uint64_t totalSize=fileSize;
	
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