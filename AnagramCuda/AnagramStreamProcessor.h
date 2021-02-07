#pragma once

#include <array>
#include <vector>

#include "AnalyzedWord.h"
#include "ThreadBlock.h"

class AnagramStreamProcessor
{
public:
    AnagramStreamProcessor(const std::string& anagramText, const SOptions& options);

    void ProcessStream(std::istream& stream);

private:
    void ExecuteThreads(size_t count);

    void Report(const std::vector<int> wordIds);

    const SOptions& m_options;
    int resultCount;
    int performance4;
    AnalyzedAnagram anagram;
    std::vector<std::string> m_strings;
    std::vector<std::vector<partialAnagramEntry>> partsByLength;
    std::vector<ThreadBlock> threads;
};
