
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
#include "ThreadBlock.h"
#include "AnagramStreamProcessor.h"

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

Soutput g_output;
int totalResults;
long long totalCalls = 0;
void handleOutputBlock(const AnalyzedWord* word, AnagramBlock& block, Soutput* output);

int main()
{
    std::ifstream infile("C:\\Users\\Ruttmann\\source\\repos\\AnagramCuda\\wordlist.txt");
    AnagramStreamProcessor controller("Best Secret Aschheim");
//    AnagramStreamProcessor controller("Best Secret Asch");

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
