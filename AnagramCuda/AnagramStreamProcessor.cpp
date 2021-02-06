#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

#include <numeric>

#include "AnagramStreamProcessor.h"

AnagramStreamProcessor::AnagramStreamProcessor(const std::string& anagramText, int threadCount)
{
    anagram.initAnagram(anagramText);
    threads.resize(threadCount);
    resultCount = 0;
}

void AnagramStreamProcessor::ProcessStream(std::ifstream& stream)
{
    std::string s;
    size_t wordIndex = 0;
    while (std::getline(stream, s))
    {
        if (s.size() <= 2)
        {
            continue;
        }

        auto& current = threads[wordIndex].m_word;
        if (current.initWord(anagram, s))
        {
            current.wordId = (decltype(current.wordId))m_strings.size();
            m_strings.emplace_back(std::move(s));

            wordIndex++;
            if (wordIndex == threads.size())
            {
                ExecuteThreads(wordIndex);
                wordIndex = 0;
            }
        }
    }

    ExecuteThreads(wordIndex);

    std::for_each(threads.begin(), threads.end(), [](ThreadBlock& thread) { thread.TerminateThread(); });

    std::cout << "1: " << std::accumulate(threads.begin(), threads.end(), 0, [](int sum, const ThreadBlock& block) { return sum + block.perfcount1; }) << std::endl;
    std::cout << "2: " << std::accumulate(threads.begin(), threads.end(), 0, [](int sum, const ThreadBlock& block) { return sum + block.perfcount2; }) << std::endl;
    std::cout << "3: " << std::accumulate(threads.begin(), threads.end(), 0, [](int sum, const ThreadBlock& block) { return sum + block.perfcount3; }) << std::endl;
    std::cout << "s: " << std::accumulate(threads.begin(), threads.end(), 0, [](int sum, const ThreadBlock& block) { return sum + block.perfcount1 - block.perfcount2 - block.perfcount3; }) << std::endl;
    std::cout << "c: " << parts.size() << std::endl;
    std::cout << "r: " << resultCount << std::endl;
}

void AnagramStreamProcessor::ExecuteThreads(size_t count)
{
    if (count == 0)
    {
        return;
    }

    // handle each word in a separate thread. splitting the array is too much overhead
    for (size_t i = 0; i < count; i++)
    {
        auto& thread = threads[i];
        thread.ScanBlockInThread(parts);
    }

    // wait until each thread is completed, do not modify the parts during the parallel phase
    size_t totalNew = 0;
    for (size_t i = 0; i < count; i++)
    {
        auto& thread = threads[i];
        thread.WaitForResult();
        totalNew += thread.m_generatedEntries.size();
    }

    // each word also must be combined with the previous words from the other threads
    std::vector<int> results(threads[0].m_results);
    auto startIndex = parts.size();
    for (size_t i = 0; i < count; i++)
    {
        auto& thread = threads[i];
        thread.CombineBlock(parts, startIndex);
        parts.insert(parts.end(), thread.m_generatedEntries.begin(), thread.m_generatedEntries.end());

        parts.resize(parts.size() + 1);
        parts.back().initFromAnalyzedWord(thread.m_word);

        for (auto result : thread.m_results)
        {
            report(thread.m_word.wordId, result);
        }

        thread.m_generatedEntries.resize(0);
        thread.m_results.resize(0);
    }
}

void AnagramStreamProcessor::report(int wordId, int moreResults)
{
    resultCount++;
    std::vector<int> wordIds{ wordId };
    while (moreResults >= 0)
    {
        wordIds.push_back(parts[moreResults].wordId);
        moreResults = parts[moreResults].previousEntry;
    }

    return;

    for (auto it = wordIds.rbegin(); it != wordIds.rend(); ++it)
    {
        std::cout << m_strings[*it] << " ";
    }

    std::cout << std::endl;
}
