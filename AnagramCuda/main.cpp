
#include <stdio.h>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>

#include <execution>

#include "CudaInputArray.h"

/// <summary>
/// Number of possible input chars. 0-9 and A-Z
/// </summary>
constexpr uint32_t possibleCharacterCount = 36;

/// <summary>
/// Number of threads inside cuda hardware
/// </summary>
constexpr auto cudaThreadCount = 512;

#include <vector>
#include <string>
#include <mutex>

using usedCharacterCount_t = int8_t;
using usedCharacterMask_t = uint32_t;

struct AnalyzedWord
{
    /// <summary>
    /// The actually used characters as bitmask
    /// </summary>
    usedCharacterMask_t usedMask;

    /// <summary>
    /// The mask for all characters that have at least one character left
    /// </summary>
    usedCharacterMask_t remainingMask;

    /// <summary>
    /// The usages per character
    /// </summary>
    usedCharacterCount_t counts[possibleCharacterCount];

    /// <summary>
    /// The id of the word
    /// </summary>
    int32_t wordId;

    /// <summary>
    /// the length of the word
    /// </summary>
    size_t length;

    /// <summary>
    /// the length until the anagram is fullfilled
    /// </summary>
    size_t restLength;

    /// <summary>
    /// The remaining count per character to fullfill the anagram
    /// </summary>
    usedCharacterCount_t remaining[possibleCharacterCount];

    /// <summary>
    /// Check these character indices (the rest is covered by the mask)
    /// </summary>
    int checkCharacters[possibleCharacterCount];

    int checkCharacterCount;

    void initAnagram(const std::string& text)
    {
        memset(&counts, 0, sizeof(counts));
        usedMask = 0;
        length = text.size();
        restLength = length;
        checkCharacterCount = 0;

        for (auto c : text)
        {
            if (isalnum(c))
            {
                auto ci = toIndex(c);
                if (counts[ci] == 0)
                {
                    checkCharacters[checkCharacterCount++] = ci;
                }

                counts[ci]++;

                auto cu = std::min(ci, (char)31);
                usedMask |= 1 << cu;
            }
        }

        memcpy(&remaining, counts, sizeof(counts));
        remainingMask = usedMask;
    }

    bool initWord(const AnalyzedWord& anagram, const std::string& text)
    {
        memset(&counts, 0, sizeof(counts));
        memcpy(&remaining, anagram.remaining, sizeof(remaining));
        remainingMask = anagram.usedMask;
        usedMask = 0;
        checkCharacterCount = anagram.checkCharacterCount;
        memcpy(checkCharacters, anagram.checkCharacters, sizeof(checkCharacters));
        length = text.size();
        restLength = anagram.length - length;

        for (auto c : text)
        {
            auto ci = toIndex(c);
            counts[ci]++;

            if (remaining[ci] == 0)
            {
                return false;
            }

            usedMask |= 1 << (std::min(ci, (char)31));
            if (remaining[ci] > 1)
            {
                remaining[ci]--;
            }
            else
            {
                remaining[ci] = 0;
                if (ci < 31)
                {
                    remainingMask ^= 1 << ci;
                }
            }
        }

        return true;
    }

private:
    inline char toIndex(char c)
    {
        if (c >= 'A')
        {
            return toupper(c) - 'A' + 10;
        }

        return c - '0';
    }
};

struct partialAnagramEntry
{
    partialAnagramEntry()
    {
    }

    /// <summary>
    /// Constructor from a singleWord
    /// </summary>
    /// <param name="word"></param>
    partialAnagramEntry(const AnalyzedWord& word)
    {
        initFromAnalyzedWord(word);
    }

    /// <summary>
    /// bitflags for all characters that have an expected count of 0
    /// </summary>
    usedCharacterMask_t doNotUseMask;

    /// <summary>
    /// Index of the previous entry for combinations. -1 for single words
    /// </summary>
    int32_t previousEntry;

    /// <summary>
    /// The id of the word in a global array
    /// </summary>
    int32_t wordId;

    /// <summary>
    /// the counts per character
    /// </summary>
    usedCharacterCount_t counts[possibleCharacterCount];

    /// <summary>
    /// total sum of characters
    /// </summary>
    int32_t restLength;

    /// <summary>
    /// copy data from an analysed word
    /// </summary>
    /// <param name="word">the word</param>
    void initFromAnalyzedWord(const AnalyzedWord& word)
    {
        memcpy(&counts, word.remaining, sizeof(counts));
        doNotUseMask = ~word.remainingMask;
        wordId = word.wordId;
        previousEntry = -1;
    }

    /// <summary>
    /// join existing data with a new word
    /// </summary>
    void joinWord(const partialAnagramEntry& entry, const AnalyzedWord& word, int index)
    {
        previousEntry = index;
        doNotUseMask = entry.doNotUseMask;
        wordId = word.wordId;
        for (int i = 0; i < possibleCharacterCount; i++)
        {
            auto count = entry.counts[i] - word.counts[i];
            counts[i] = count;
            if (count == 0)
            {
                doNotUseMask |= i < 32 ? (1 << i) : (1 << 31);
            }
        }
    }
};

constexpr auto outputSizeBuffer = 512;
struct Soutput
{
    int callCount;
    std::vector<int> results;
    std::vector<partialAnagramEntry> outputs;
};

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

using AnagramBlock = CudaInputArray<partialAnagramEntry>;

CudaInputArray<partialAnagramEntry> parts;
std::vector<std::string> strings;

void reportResult(int wordId, int moreResults)
{
    std::vector<int> wordIds{ wordId };
    while (moreResults >= 0)
    {
        wordIds.push_back(parts.data[moreResults].wordId);
        moreResults = parts.data[moreResults].previousEntry;
    }

    for (auto it = wordIds.rbegin(); it != wordIds.rend(); ++it)
    {
        std::cout << strings[*it] << " ";
    }

    std::cout << std::endl;
}

Soutput g_output;
int totalResults;
long long totalCalls = 0;
cudaError_t cudaAnagram(AnalyzedWord* word, AnagramBlock& block, Soutput* output);
void handleOutputBlock(const AnalyzedWord* word, AnagramBlock& block, Soutput* output);

int main()
{
    AnalyzedWord anagram;
    AnalyzedWord current;

    std::ifstream infile("C:\\Users\\Ruttmann\\source\\repos\\AnagramCuda\\wordlist.txt");
    // anagram.initAnagram("Best Secret Aschheim");
    anagram.initAnagram("Best Secret Aschheim");
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
}

void handleOutputBlock(const AnalyzedWord* word, AnagramBlock& block, Soutput* output)
{
    parts.data.insert(parts.data.end(), output->outputs.begin(), output->outputs.end());

    for (auto result: output->results)
    {
        totalResults++;
        reportResult(word->wordId, result);
    }

    output->outputs.resize(0);
    output->results.resize(0);
}
