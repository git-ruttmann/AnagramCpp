#pragma once

#include <string>

struct SOptions
{
	int MinWordLength = 2;
	int ThreadCount = 8;
	bool PrintResults = true;
	bool PrintPerformanceCounters = false;
	std::string path;
	std::string angramText;

	bool ParseArguments(int argc, const char ** argv)
	{
		if (argc < 3)
		{
			return false;
		}

		path = argv[1];
		angramText = argv[2];

		for (int i = 3; i < argc; i++)
		{
			std::string arg{ argv[i] };
			if (arg == "-q")
			{
				PrintResults = false;
			}

			if (arg == "-s")
			{
				PrintPerformanceCounters = true;
			}
		}
	
		return true;
	}
};