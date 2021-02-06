#include <iostream>
#include <fstream>

#include "AnagramStreamProcessor.h"

int main()
{
    std::ifstream infile("C:\\Users\\Ruttmann\\source\\repos\\AnagramCuda\\wordlist.txt");
    AnagramStreamProcessor controller("Best Secret Aschheim");
//    AnagramStreamProcessor controller("Best Secret Asch");

    controller.ProcessStream(infile);
    return 0;
}