#include <iostream>
#include <fstream>
#include <thread>
#include <algorithm>

#include "AnagramStreamProcessor.h"

int main(int argc, const char ** argv)
{
    SOptions options;
	if (!options.ParseArguments(argc, argv))
	{
		return -1;
	}

    //std::ifstream infile("C:\\Users\\Ruttmann\\source\\repos\\AnagramCuda\\wordlist.txt");
    std::ifstream infile(options.path);

    options.ThreadCount = std::max(2u, std::thread::hardware_concurrency());
    options.PrintPerformanceCounters = true;
    AnagramStreamProcessor controller(options);
//    AnagramStreamProcessor controller("Best Secret Asch");

    controller.ProcessStream(infile);
    return 0;
}