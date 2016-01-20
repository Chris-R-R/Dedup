# Dedup
A Windows command line tool for to identify and optionally remove duplicate files



Usage: Dedup <folder> <options>

This program will find duplicate files within a folder

Options:

-D - delete duplicates, keeping the file with the oldest creation date

-S - show each duplicate

-L - create links for duplicates

The idea for this tool came to me because I was copying pictures from my iphone to my computer, then later on copying them again as I took
more and more pictures. After a while I realized I was getting a whole bunch of duplicate pictures on my computer. After a while it became
unfeasible to manually find and delete duplicate pictures so I created this tool to automate the process.

It can be used for any file types, I just happen to use it for pictures. It doesn't use the file name to determine if a file is a copy,
it goes by file contents.
