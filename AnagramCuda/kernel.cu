
#include "cuda_runtime.h"
#include "device_launch_parameters.h"

#include <stdio.h>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>

#include "CudaInputArray.h"

/// <summary>
/// Number of possible input chars. 0-9 and A-Z
/// </summary>
constexpr uint32_t possibleCharacterCount = 36;

#include <vector>
#include <string>

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
    /// The remaining count per character to fullfill the anagram
    /// </summary>
    usedCharacterCount_t remaining[possibleCharacterCount];

    void initAnagram(const std::string& text)
    {
        memset(&counts, 0, sizeof(counts));
        usedMask = 0;
        length = text.size();

        for (auto c : text)
        {
            if (isalnum(c))
            {
                auto ci = toIndex(c);
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

constexpr auto outputSizeBuffer = 1<<18;
struct Soutput
{
    int results[outputSizeBuffer];
    partialAnagramEntry output[outputSizeBuffer];
    int outputCount;
    int overflow;
    int resultCount;
    int callCount;
};

__device__ __host__ void doHandleBlock(const AnalyzedWord* word, int index, const partialAnagramEntry & entry, Soutput* output)
{
#ifdef  __CUDA_ARCH__
    atomicAdd(&output->callCount, 1);
#else
    output->callCount++;
#endif

    if (entry.doNotUseMask & word->usedMask)
    {
        return;
    }

    usedCharacterCount_t maskOfSum = 0;
    for (size_t i = 0; i < possibleCharacterCount; i++)
    {
        maskOfSum |= (entry.counts[i] - word->counts[i]);
    }

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
    else if (maskOfSum > 0)
    {
        // valid combination
#ifdef  __CUDA_ARCH__
        auto& target = output->output[atomicAdd(&output->outputCount, 1)];
#else
        auto& target = output->output[output->outputCount++];
#endif
        if (output->outputCount >= outputSizeBuffer)
        {
            output->overflow = true;
            return;
        }

        target.joinWord(entry, *word, index);
    }
}

__global__ void handleBlock(AnalyzedWord* word, const partialAnagramEntry* block, Soutput * output)
{
    const auto index = blockIdx.x * blockDim.x + threadIdx.x;
    const partialAnagramEntry& entry = block[index];
    doHandleBlock(word, index, entry, output);
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

Soutput output;
cudaError_t cudaAnagram(AnalyzedWord* word, AnagramBlock & block, Soutput* output);
int main()
{
    AnalyzedWord anagram;
    AnalyzedWord current;
    int totalResults = 0;

    std::ifstream infile("C:\\Users\\Ruttmann\\source\\repos\\AnagramCuda\\wordlist.txt");
    // anagram.initAnagram("Best Secret Aschheim");
    anagram.initAnagram("Best Secret Aschheim");
    // anagram.initAnagram("012");

    std::string s;
    output.callCount = 0;
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

            output.resultCount = 0;
            output.outputCount = 0;
            output.overflow = 0;
#if true
            auto cudaStatus = cudaAnagram(&current, parts, &output);
            if (cudaStatus != cudaSuccess)
            {
                std::cerr << "cudaAnagram failed " << cudaStatus << std::endl;
                return 1;
            }
#else
            for (size_t i = 0; i < parts.data.size(); i++)
            {
                doHandleBlock(&current, i, parts.data[i], &output);
            }
#endif

            if (output.overflow)
            {
                reportResult(current.wordId, -1);
                std::cerr << "Too many results: " << output.outputCount << " " << output.resultCount << std::endl;
            }

            parts.data.emplace_back(current);

            for (size_t i = 0; i < std::min(outputSizeBuffer, output.outputCount); i++)
            {
                parts.data.push_back(output.output[i]);
            }

            for (size_t i = 0; i < ::min(outputSizeBuffer, output.resultCount); i++)
            {
                totalResults++;
//                reportResult(current.wordId, output.results[i]);
            }
        }
    }

    std::cout << "found " << totalResults << std::endl;
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

    uploadToCuda(cudaStatus, dev_word, word);
    uploadToCuda(cudaStatus, dev_output, output);
    block.Upload();

    auto blockCount = block.data.size() / 1024 + 1;
    if (blockCount > 1)
    {
        handleBlock <<<blockCount - 1, 1024>>> (dev_word, block.dev_memory, dev_output);
    }

    handleBlock<<<1, block.data.size() % 1024>>>(dev_word, block.dev_memory, dev_output);

    cudaStatus = cudaDeviceSynchronize();
    cudaStatus = cudaMemcpy(output, dev_output, sizeof(Soutput), cudaMemcpyDeviceToHost);

    cudaFree(dev_word);
    cudaFree(dev_output);

    return cudaStatus;
}
