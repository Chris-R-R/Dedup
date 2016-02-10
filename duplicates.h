class oneFile
{
	public:
		std::wstring path;
		std::wstring name;
		uint64_t timeStamp;
};

std::map<uint64_t,std::list<oneFile>> g_files;	//Map of sizes, list of objects
void RecurseFilePath(std::wstring path);
uint32_t CalculateFileHash(const std::wstring& newPath, uint64_t fileSize);
void checkDuplicates(bool deleteFiles,bool showDuplicates,bool linkFiles);
bool AreDuplicates(const std::wstring& file1, const std::wstring& file2, uint64_t fileSize);