#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

#include <numeric>

#include "AnagramStreamProcessor.h"

AnagramStreamProcessor::AnagramStreamProcessor(const std::string& anagramText, int threadCount)
{
    anagram.initAnagram(anagramText);
    threads.reserve(threadCount);
    for (size_t i = 0; i < threadCount; i++)
    {
        threads.emplace_back(ThreadBlock());
    }

    partsByLength.resize(anagram.length);
    resultCount = 0;
    performance4 = 0;

    // this is a handcrafted optimization to reduce reallocation
    for (size_t i = 2; i < anagram.length; i++)
    {
        partsByLength[i].reserve(i < 6 ? 1000000 : (i < 10 ? 50000 : 10000));
    }
}

void AnagramStreamProcessor::ProcessStream(std::istream& stream)
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

    std::cout << "t:";
    size_t lengthSum = 0;
    for (const auto & partialLength : partsByLength)
    {
        std::cout << " " << partialLength.size();
        lengthSum += partialLength.size();
    }
    std::cout << " t: " << lengthSum << std::endl;

    // std::cout << "c: " << parts.size() << std::endl;
    std::cout << "r: " << resultCount << std::endl;
    std::cout << "p4:" << performance4 << std::endl;
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
        thread.ScanBlockInThread(partsByLength);
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
    std::vector<size_t> handledIndizis;
    for (const auto& part : partsByLength)
    {
        handledIndizis.push_back(part.size());
    }

    for (size_t i = 0; i < count; i++)
    {
        auto& thread = threads[i];
        for (size_t j = 0; j < partsByLength.size(); j++)
        {
            thread.CombineBlock(partsByLength[j], handledIndizis[j]);
        }

        for (size_t j = 0; j < thread.m_generatedEntries.size(); j++)
        {
            const auto& entry = thread.m_generatedEntries[j];
            auto& list = partsByLength[entry.restLength];
            if (list.size() == list.capacity())
            {
                performance4++;
            }

            partsByLength[entry.restLength].push_back(entry);
        }

        auto& lengthData = partsByLength[thread.m_word.restLength];
        lengthData.resize(lengthData.size() + 1);
        lengthData.back().initFromAnalyzedWord(thread.m_word);

        for (size_t j = 0; j < thread.m_resultWordId.size(); j++)
        {
            report(thread.m_word.wordId, thread.m_resultWordId[j], thread.m_resultWordLength[j]);
        }

        thread.m_generatedEntries.resize(0);
        thread.m_resultWordId.resize(0);
        thread.m_resultWordLength.resize(0);
    }
}

void AnagramStreamProcessor::report(int wordId, int moreResults, int moreLength)
{
    resultCount++;
    std::vector<int> wordIds{ wordId };
    while (moreResults >= 0)
    {
        const auto& entry = partsByLength[moreLength][moreResults];
        wordIds.push_back(entry.wordId);
        moreResults = entry.previousEntry;
        moreLength = entry.previousLength;
    }

    return;

    for (auto it = wordIds.rbegin(); it != wordIds.rend(); ++it)
    {
        std::cout << m_strings[*it] << " ";
    }

    std::cout << std::endl;
}
