#pragma once

#include <array>
#include <vector>

#include "AnalyzedWord.h"
#include "ThreadBlock.h"

class AnagramStreamProcessor
{
public:
    AnagramStreamProcessor(const std::string& anagramText, int threadCount = 8);

    void ProcessStream(std::ifstream& stream);

private:
    void ExecuteThreads(size_t count);

    void report(int wordId, int moreResults);

    AnalyzedWord anagram;
    std::vector<std::string> m_strings;
    std::vector<partialAnagramEntry> parts;
    std::vector<ThreadBlock> threads;
};
