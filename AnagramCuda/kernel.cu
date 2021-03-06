﻿
#if false

#include "cuda_runtime.h"
#include "device_launch_parameters.h"

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
    __host__ __device__
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
    int outputCount;
    int overflow;
    int resultCount;
    int callCount;
    int results[outputSizeBuffer];
    partialAnagramEntry output[outputSizeBuffer + cudaThreadCount];
};

__device__ __host__ void doHandleBlock(const AnalyzedWord word, int index, const partialAnagramEntry & entry, Soutput* output)
{
#ifdef  __CUDA_ARCH__
    atomicAdd(&output->callCount, 1);
#else
    output->callCount++;
#endif

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
#ifdef  __CUDA_ARCH__
        auto resultIndex = atomicAdd(&output->resultCount, 1);
#else
        auto resultIndex = output->resultCount++;
#endif
        if (resultIndex >= outputSizeBuffer)
        {
            output->overflow = true;
            return;
        }

        // valid result
        output->results[resultIndex] = index;
    }
    else if ((maskOfSum > 0) && (entry.restLength - word.length > 2))
    {
        // valid combination
#ifdef  __CUDA_ARCH__
        auto& target = output->output[atomicAdd(&output->outputCount, 1)];
#else
        auto& target = output->output[output->outputCount++];
#endif
        if (output->outputCount >= outputSizeBuffer + cudaThreadCount)
        {
            output->overflow = true;
            return;
        }

        target.joinWord(entry, word, index);
    }
}

__global__ void handleBlock(const AnalyzedWord* word, int max, const partialAnagramEntry* block, Soutput * output)
{
    const auto index = blockIdx.x * blockDim.x + threadIdx.x;
    if (index >= max)
    {
        return;
    }

    const partialAnagramEntry& entry = block[index];
    doHandleBlock(*word, index, entry, output);
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
cudaError_t cudaAnagram(AnalyzedWord* word, AnagramBlock & block, Soutput* output);
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

            g_output.resultCount = 0;
            g_output.outputCount = 0;
            g_output.overflow = 0;
#if false
            auto cudaStatus = cudaAnagram(&current, parts, &g_output);
            if (cudaStatus != cudaSuccess)
            {
                std::cerr << "cudaAnagram failed " << cudaStatus << std::endl;
                return 1;
            }
#else
            auto& a = parts.data;
            std::mutex m;
            std::for_each(
                std::execution::par_unseq,
                std::begin(a), 
                std::end(a), 
                [&m, &current](auto & value)
                {
                    {
                        std::lock_guard<std::mutex> guard(m);
                        totalCalls++;
                    }

                    int idx = 0;
                    doHandleBlock(current, idx, value, &g_output);
                });

            auto end = parts.data.size();
            for (size_t i = 0; i < end; i++)
            {
                totalCalls++;
                doHandleBlock(current, i, parts.data[i], &g_output);
                if (g_output.outputCount > outputSizeBuffer)
                {
                    handleOutputBlock(&current, parts, &g_output);
                }
            }

            handleOutputBlock(&current, parts, &g_output);
#endif
            parts.data.emplace_back(current);
        }
    }

    std::cout << "found " << totalResults << " " << parts.data.size() << " " << totalCalls << " " << g_output.callCount << std::endl;
}

template<typename T>
void uploadToCuda(cudaError_t& error, T& devicePtr, const T data)
{
    if (error != cudaSuccess)
    {
        return;
    }

    constexpr auto size = sizeof(decltype(*devicePtr));
    error = cudaMalloc((void**)&devicePtr, size);
    if (error != cudaSuccess)
    {
        return;
    }

    error = cudaMemcpy(devicePtr, data, size, cudaMemcpyHostToDevice);
}

void handleOutputBlock(const AnalyzedWord* word, AnagramBlock& block, Soutput* output)
{
    if (output->overflow)
    {
        reportResult(word->wordId, -1);
        std::cerr << "Too many results: " << output->outputCount << " " << output->resultCount << std::endl;
    }

    for (size_t i = 0; i < std::min(outputSizeBuffer + cudaThreadCount, output->outputCount); i++)
    {
        parts.data.push_back(output->output[i]);
    }

    for (size_t i = 0; i < std::min(outputSizeBuffer, output->resultCount); i++)
    {
        totalResults++;
        reportResult(word->wordId, output->results[i]);
    }

    output->resultCount = 0;
    output->outputCount = 0;
}

cudaError_t cudaAnagram(AnalyzedWord* word, AnagramBlock& block, Soutput* output)
{
    AnalyzedWord* dev_word = NULL;
    Soutput* dev_output = NULL;

    // Choose which GPU to run on, change this on a multi-GPU system.
    auto cudaStatus = cudaSetDevice(0);
    if (cudaStatus != cudaSuccess) {
        std::cerr << "cudaSetDevice failed!  Do you have a CUDA-capable GPU installed?" << std::endl;
        return cudaStatus;
    }

    constexpr auto oututExchangeSize = 16;
    uploadToCuda(cudaStatus, dev_word, word);
    uploadToCuda(cudaStatus, dev_output, output);
    block.Upload();

#if false
    auto blockSize = block.data.size();
    for (size_t offset = 0; offset < blockSize; offset += cudaThreadCount)
    {
        auto count = std::min(blockSize - offset, cudaThreadCount);
        handleBlock<<<1, count>>> (dev_word, block.dev_memory + offset, dev_output);
        cudaStatus = cudaDeviceSynchronize();

        // copy only the counters and do intermediate reporting
        cudaMemcpy(output, dev_output, oututExchangeSize, cudaMemcpyDeviceToHost);
        if (output->outputCount >= outputSizeBuffer || output->resultCount >= outputSizeBuffer)
        {
            cudaStatus = cudaMemcpy(output, dev_output, sizeof(Soutput), cudaMemcpyDeviceToHost);
            handleOutputBlock(word, block, output);
            cudaStatus = cudaMemcpy(output, dev_output, oututExchangeSize, cudaMemcpyHostToDevice);
        }
    }
#else
    handleBlock<<<block.data.size() / cudaThreadCount + 1, cudaThreadCount>>>(
        dev_word, block.data.size(), block.dev_memory, dev_output);
#endif

    cudaStatus = cudaDeviceSynchronize();
    cudaStatus = cudaMemcpy(output, dev_output, sizeof(Soutput), cudaMemcpyDeviceToHost);
    handleOutputBlock(word, block, output);

    cudaFree(dev_word);
    cudaFree(dev_output);

    return cudaStatus;
}

#endif