#pragma once

#include <ctype.h>
#include <array>
#include <string>

/// <summary>
/// Number of possible input chars. 0-9 and A-Z
/// </summary>
constexpr uint32_t possibleCharacterCount = 36;

/// <summary>
/// type to count the occurences per character
/// </summary>
using usedCharacterCount_t = int8_t;

/// <summary>
/// type for the mask of existing characters
/// </summary>
using usedCharacterMask_t = uint32_t;

struct AnalyzedAnagram
{
    /// <summary>
    /// The actually used characters as bitmask
    /// </summary>
    usedCharacterMask_t usedMask;

    /// <summary>
    /// The usages per character
    /// </summary>
    usedCharacterCount_t counts[possibleCharacterCount];

    /// <summary>
    /// the length of the word
    /// </summary>
    int32_t length;

    void initAnagram(const std::string& text)
    {
        memset(&counts, 0, sizeof(counts));
        usedMask = 0;
        length = 0;
        usedCharacterCount = 0;
        for (size_t i = 0; i < indexTable.size(); i++)
        {
            indexTable[i] = -1;
        }

        for (unsigned char c : text)
        {
            if (isalnum(c))
            {
                c = tolower(c);
                if (indexTable[c] == -1)
                {
                    indexTable[c] = usedCharacterCount++;
                }

                auto ci = ToIndex(c);

                length++;
                counts[ci]++;

                auto cu = std::min(ci, 31);
                usedMask |= 1 << cu;
            }
        }
    }

    int32_t ToIndex(char c) const
    {
        return indexTable[(unsigned char)tolower(c)];
    }

    int GetUsedCharacterCount() const
    {
        return usedCharacterCount;
    }

private:
    std::array<int32_t, 256> indexTable;
    int usedCharacterCount;
};

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
    int32_t length;

    /// <summary>
    /// the length until the anagram is fullfilled
    /// </summary>
    int32_t restLength;

    /// <summary>
    /// The remaining count per character to fullfill the anagram
    /// </summary>
    usedCharacterCount_t remaining[possibleCharacterCount];

    /// <summary>
    /// number of different characters used (== number of used indizes in counts/remaining)
    /// </summary>
    int numberOfCharacterClasses;

    bool initWord(const AnalyzedAnagram& anagram, const std::string& text)
    {
        memset(&counts, 0, sizeof(counts));
        memcpy(&remaining, anagram.counts, sizeof(remaining));
        remainingMask = anagram.usedMask;
        usedMask = 0;
        numberOfCharacterClasses = anagram.GetUsedCharacterCount();
        length = (int)text.size();
        restLength = anagram.length - length;

        for (auto c : text)
        {
            auto ci = anagram.ToIndex(c);
            if (ci < 0)
            {
                return false;
            }

            counts[ci]++;

            if (remaining[ci] == 0)
            {
                return false;
            }

            usedMask |= 1 << (std::min(ci, 31));
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
};
