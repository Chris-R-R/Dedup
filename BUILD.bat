@REM 64 bit:
@REM call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\x86_amd64\vcvarsx86_amd64.bat"
@REM 32 bit:
@REM call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\vcvars32.bat"

cl duplicates.cpp PMurHash.c /O2 /EHsc /GA /MT /FeDedup.exe