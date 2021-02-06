
#include <stdio.h>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <array>
#include <iostream>
#include <fstream>

#include <execution>

#include "AnalyzedWord.h"
#include "PartialAnagram.h"
#include "CudaInputArray.h"

/// <summary>
/// Number of threads inside cuda hardware
/// </summary>
constexpr auto cudaThreadCount = 512;

#include <vector>
#include <string>
#include <mutex>

using AnagramBlock = CudaInputArray<partialAnagramEntry>;

CudaInputArray<partialAnagramEntry> parts;
std::vector<std::string> strings;

constexpr auto outputSizeBuffer = 512;
struct Soutput
{
    int callCount;
    std::vector<int> results;
    std::vector<partialAnagramEntry> outputs;
};

class ThreadBlock;
void executeThread(ThreadBlock* threadBlock);

class ThreadBlock
{
public:
    ThreadBlock()
    {
        m_data = NULL;
        m_finishThread = false;
        m_stage = 0;
        m_dataReady = false;
        m_dataCompleted = false;
        m_thread = std::thread(executeThread, this);
    }

    void RunThread()
    {
        m_stage = 1;
        while (!m_finishThread)
        {
            m_stage = 2;
            {
                m_stage = 3;
                std::unique_lock<std::mutex> uniqueLock(m_lock);
                m_dataReadyCondition.wait(uniqueLock, [&] { return m_finishThread || m_dataReady; });
                m_dataReady = false;
            }

            m_stage = 4;
            if (!m_finishThread)
            {
                ProcessBlock(0);
            }

            m_stage = 5;
            {
                m_stage = 6;
                std::lock_guard guard(m_lock);
                m_dataCompleted = true;
                m_dataCompletedCondition.notify_one();
            }
        }
    }

    /// <summary>
    /// combining the search word with every entry in the data block. Run in a separate thread.
    /// </summary>
    /// <param name="word">the search word</param>
    /// <param name="data">the data block. MUST NOT MODIFY until WaitForResult returns</param>
    void ScanBlockInThread(const AnalyzedWord & word, const std::vector<partialAnagramEntry>& data)
    {
        std::lock_guard guard(m_lock);
        m_word = &word;
        m_data = &data;
        m_dataReady = true;
        m_dataCompleted = false;
        m_dataReadyCondition.notify_one();
    }

    void WaitForResult()
    {
        std::unique_lock<std::mutex> uniqueLock(m_lock);
        m_dataCompletedCondition.wait(uniqueLock, [&] { return m_dataCompleted; });
    }

    void TerminateThread()
    {
        {
            std::lock_guard guard(m_lock);
            m_word = NULL;
            m_data = NULL;
            m_finishThread = true;
            m_dataReadyCondition.notify_one();
        }

        WaitForResult();
    }

    void CombineBlock(const AnalyzedWord& word, const std::vector<partialAnagramEntry>& data, size_t startOffset)
    {
        m_word = &word;
        m_data = &data;
        ProcessBlock(startOffset);
    }

    /// <summary>
    /// new partial combinations for the current word
    /// </summary>
    std::vector<partialAnagramEntry> m_generatedEntries;

    /// <summary>
    /// indizes in data that form a valid result together with the currently scanned word
    /// </summary>
    std::vector<int> m_results;

private:

    void ProcessBlock(size_t startOffset)
    {
        const auto& word = *m_word;
        auto arraySize = m_data->size();
        for (size_t i = startOffset; i < arraySize; i++)
        {
            const auto& entry = (*m_data)[i];
            if (entry.doNotUseMask & word.usedMask)
            {
                continue;
            }

            usedCharacterCount_t maskOfSum = 0;
            for (size_t j = 0; j < possibleCharacterCount; j++)
            {
                maskOfSum |= (entry.counts[j] - word.counts[j]);
            }

            if (maskOfSum == 0)
            {
                m_results.emplace_back((int)i);
            }
            else if ((maskOfSum > 0) && (entry.restLength - word.length > 2))
            {
                m_generatedEntries.resize(m_generatedEntries.size() + 1);
                m_generatedEntries.back().joinWord(entry, word, (int)i);
            }
        }
    }

    const AnalyzedWord * m_word;
    const std::vector<partialAnagramEntry>* m_data;
    bool m_dataReady;
    bool m_dataCompleted;

    int m_stage;
    std::mutex m_lock;
    std::condition_variable m_dataReadyCondition;
    std::condition_variable m_dataCompletedCondition;
    bool m_finishThread;
    std::thread m_thread;
};

void executeThread(ThreadBlock* threadBlock)
{
    threadBlock->RunThread();
}

void doHandleBlock(const AnalyzedWord word, int index, const partialAnagramEntry& entry, Soutput* output, std::mutex & m)
{
    output->callCount++;

    if (entry.doNotUseMask & word.usedMask)
    {
        return;
    }

#if false
    usedCharacterCount_t maskOfSum = 0;
    for (auto i = 0; i < word.checkCharacterCount; i++)
    {
        auto ci = word.checkCharacters[i];
        maskOfSum |= (entry.counts[ci] - word.counts[ci]);
    }
#else
    usedCharacterCount_t maskOfSum = 0;
    for (size_t i = 0; i < possibleCharacterCount; i++)
    {
        maskOfSum |= (entry.counts[i] - word.counts[i]);
    }
#endif

    if (maskOfSum == 0)
    {
        std::lock_guard<std::mutex> guard(m);
        output->results.emplace_back(index);
    }
    else if ((maskOfSum > 0) && (entry.restLength - word.length > 2))
    {
        size_t resultIndex;
        {
            std::lock_guard<std::mutex> guard(m);
            resultIndex = output->outputs.size();
            output->outputs.resize(output->outputs.size() + 1);
        }

        // valid combination
        auto& target = output->outputs[resultIndex];
        target.joinWord(entry, word, index);
    }
}

void reportResult(int wordId, int moreResults)
{
    std::vector<int> wordIds{ wordId };
    while (moreResults >= 0)
    {
        wordIds.push_back(parts.data[moreResults].wordId);
        moreResults = parts.data[moreResults].previousEntry;
    }

//    return;

    for (auto it = wordIds.rbegin(); it != wordIds.rend(); ++it)
    {
        std::cout << strings[*it] << " ";
    }

    std::cout << std::endl;
}

constexpr auto ThreadCount = 8;
class AnagramBlockController
{
public:
    AnagramBlockController(const std::string & anagramText)
    {
        anagram.initAnagram(anagramText);
    }

    void ProcessStream(std::ifstream& stream)
    {
        std::string s;
        int wordIndex = 0;
        while (std::getline(stream, s))
        {
            if (s.size() <= 2)
            {
                continue;
            }

            auto& current = words[wordIndex];
            if (current.initWord(anagram, s))
            {
                current.wordId = (decltype(current.wordId))m_strings.size();
                m_strings.emplace_back(std::move(s));

                wordIndex++;
                if (wordIndex == ThreadCount)
                {
                    handleWords(wordIndex);
                    wordIndex = 0;
                }
            }
        }

        handleWords(wordIndex);

        std::for_each(threads.begin(), threads.end(), [](ThreadBlock& thread) { thread.TerminateThread(); });
    }

private:
    void handleWords(int count)
    {
        if (count == 0)
        {
            return;
        }

        // handle each word in a separate thread. splitting the array is too much overhead
        for (size_t i = 0; i < count; i++)
        {
            const auto& word = words[i];
            auto& thread = threads[i];
            thread.ScanBlockInThread(word, parts);
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
            const auto& word = words[i];
            auto& thread = threads[i];
            thread.CombineBlock(word, parts, startIndex);
            parts.insert(parts.end(), thread.m_generatedEntries.begin(), thread.m_generatedEntries.end());

            parts.resize(parts.size() + 1);
            parts.back().initFromAnalyzedWord(word);

            for (auto result : thread.m_results)
            {
                report(word.wordId, result);
            }

            thread.m_generatedEntries.resize(0);
            thread.m_results.resize(0);
        }
    }

    void report(int wordId, int moreResults)
    {
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

    AnalyzedWord anagram;
    std::vector<std::string> & m_strings = strings;
    AnalyzedWord words[ThreadCount];
    std::vector<partialAnagramEntry> parts;
    std::array<ThreadBlock, ThreadCount> threads;
};

Soutput g_output;
int totalResults;
long long totalCalls = 0;
void handleOutputBlock(const AnalyzedWord* word, AnagramBlock& block, Soutput* output);

int main()
{
    std::ifstream infile("C:\\Users\\Ruttmann\\source\\repos\\AnagramCuda\\wordlist.txt");
    AnagramBlockController controller("Best Secret Aschheim");
//    AnagramBlockController controller("Best Secret Asch");

    controller.ProcessStream(infile);
    return 0;
}

int main2()
{
    AnalyzedWord anagram;
    AnalyzedWord current;

    std::ifstream infile("C:\\Users\\Ruttmann\\source\\repos\\AnagramCuda\\wordlist.txt");
    // anagram.initAnagram("Best Secret Aschheim");
    anagram.initAnagram("Best Secret");
    // anagram.initAnagram("012");

    std::string s;
    g_output.callCount = 0;
    totalResults = 0;
    parts.data.reserve(500000);

    while (std::getline(infile, s))
    {
        if (s.size() <= 2)
        {
            continue;
        }

        if (current.initWord(anagram, s))
        {
            //            std::cerr << "consider " << s << std::endl;
            current.wordId = (decltype(current.wordId))strings.size();
            strings.emplace_back(std::move(s));

            if (current.remainingMask == 0)
            {
                reportResult(current.wordId, -1);
                continue;
            }

            std::mutex m;
            auto& a = parts.data;
            auto executor = [&](const auto& value)
            {
                int idx = (int)(&value - &a[0]);
                doHandleBlock(current, idx, value, &g_output, m);
            };
#if true
            if (a.size() < 1000000)
            {
                std::for_each(std::execution::seq, std::begin(a), std::end(a), executor);
            }
            else
            {
                std::for_each(std::execution::par_unseq, std::begin(a), std::end(a), executor);
            }
#else
            auto end = parts.data.size();
            for (size_t i = 0; i < end; i++)
            {
                totalCalls++;
                doHandleBlock(current, i, parts.data[i], &g_output, m);
            }
#endif

            handleOutputBlock(&current, parts, &g_output);
            parts.data.emplace_back(current);
        }
    }

    std::cout << "found " << totalResults << " " << parts.data.size() << " " << totalCalls << " " << g_output.callCount << std::endl;
    return 0;
}

void handleOutputBlock(const AnalyzedWord* word, AnagramBlock& block, Soutput* output)
{
//    std::cout << strings[word->wordId] << ": " << output->outputs.size() << std::endl;
    parts.data.insert(parts.data.end(), output->outputs.begin(), output->outputs.end());

    for (auto result: output->results)
    {
        totalResults++;
        reportResult(word->wordId, result);
    }

    output->outputs.resize(0);
    output->results.resize(0);
}
