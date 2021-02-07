#include <iostream>
#include <fstream>
#include <thread>
#include <algorithm>

#include "AnagramStreamProcessor.h"

int main()
{
    SOptions options;
    //std::ifstream infile("C:\\Users\\Ruttmann\\source\\repos\\AnagramCuda\\wordlist.txt");
    std::ifstream infile("C:\\Users\\Ruttmann\\source\\repos\\AnagramCuda\\wordlist.txt");

    options.ThreadCount = std::max(2u, std::thread::hardware_concurrency());
    options.PrintResults = true;
    AnagramStreamProcessor controller("Best Secret Aschheim", options);
//    AnagramStreamProcessor controller("Best Secret Asch");

    controller.ProcessStream(infile);
    return 0;
}