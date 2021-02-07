#include <iostream>
#include <fstream>

#include "AnagramStreamProcessor.h"

int main()
{
    SOptions options;
    //std::ifstream infile("C:\\Users\\Ruttmann\\source\\repos\\AnagramCuda\\wordlist.txt");
    std::ifstream infile("C:\\Users\\Ruttmann\\source\\repos\\AnagramCuda\\wordlist.txt");

    options.ThreadCount = 8;
    AnagramStreamProcessor controller("Best Secret Aschheim", options);
//    AnagramStreamProcessor controller("Best Secret Asch");

    controller.ProcessStream(infile);
    return 0;
}