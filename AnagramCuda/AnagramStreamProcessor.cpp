#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

#include <numeric>

#include "AnagramStreamProcessor.h"

AnagramStreamProcessor::AnagramStreamProcessor(const std::string& anagramText, const SOptions& options)
    : m_options(options)
{
    anagram.initAnagram(anagramText);
    threads.reserve(options.ThreadCount);
    for (size_t i = 0; i < options.ThreadCount; i++)
    {
        threads.emplace_back(options, partsByLength);
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

    if (m_options.PrintPerformanceCounters)
    {
        std::cout << "1: " << std::accumulate(threads.begin(), threads.end(), 0, [](int sum, const ThreadBlock& block) { return sum + block.perfcount1; }) << std::endl;
        std::cout << "2: " << std::accumulate(threads.begin(), threads.end(), 0, [](int sum, const ThreadBlock& block) { return sum + block.perfcount2; }) << std::endl;
        std::cout << "3: " << std::accumulate(threads.begin(), threads.end(), 0, [](int sum, const ThreadBlock& block) { return sum + block.perfcount3; }) << std::endl;
        std::cout << "s: " << std::accumulate(threads.begin(), threads.end(), 0, [](int sum, const ThreadBlock& block) { return sum + block.perfcount1 - block.perfcount2 - block.perfcount3; }) << std::endl;

        std::cout << "t:";
        size_t lengthSum = 0;
        for (const auto& partialLength : partsByLength)
        {
            std::cout << " " << partialLength.size();
            lengthSum += partialLength.size();
        }
        std::cout << " t: " << lengthSum << std::endl;

        // std::cout << "c: " << parts.size() << std::endl;
        std::cout << "r: " << resultCount << std::endl;
        std::cout << "p4:" << performance4 << std::endl;
    }
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
        threads[i].ScanBlockInThread();
    }

    // wait until each thread is completed, do not modify the parts during the parallel phase
    for (size_t i = 0; i < count; i++)
    {
        threads[i].WaitForResult();
    }

    // capture the number of already handled items per part array
    std::vector<size_t> handledIndizis;
    handledIndizis.reserve(partsByLength.size());
    for (const auto& part : partsByLength)
    {
        handledIndizis.push_back(part.size());
    }

    // each word also must be combined with the previous words from the other threads
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

        for (const std::vector<int> wordIds: thread.m_results)
        {
            Report(wordIds);
        }

        thread.m_generatedEntries.resize(0);
        thread.m_results.resize(0);
    }
}

void AnagramStreamProcessor::Report(const std::vector<int> wordIds)
{
    resultCount++;

    if (m_options.PrintResults)
    {
        std::string text;
        text.reserve(anagram.length + wordIds.size());

        for (auto it = wordIds.rbegin(); it != wordIds.rend(); ++it)
        {
            text.append(m_strings[*it]);
            text.push_back(' ');
        }

        std::cout << text << std::endl;
    }
}
