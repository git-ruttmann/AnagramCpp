#pragma once

#include <array>
#include <vector>

#include "AnalyzedWord.h"
#include "ThreadBlock.h"

class AnagramStreamProcessor
{
public:
    AnagramStreamProcessor(const std::string& anagramText, int threadCount = 16);

    void ProcessStream(std::istream& stream);

private:
    void ExecuteThreads(size_t count);

    void report(int wordId, int moreResults, int moreLength);

    int resultCount;
    AnalyzedWord anagram;
    std::vector<std::string> m_strings;
    std::vector<std::vector<partialAnagramEntry>> partsByLength;
    std::vector<ThreadBlock> threads;
};
