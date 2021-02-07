#pragma once

#include <ctype.h>

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
    /// Check these character indices (the rest is covered by the mask)
    /// </summary>
    int checkCharacters[possibleCharacterCount];

    int checkCharacterCount;

    void initAnagram(const std::string& text)
    {
        memset(&counts, 0, sizeof(counts));
        usedMask = 0;
        length = 0;
        checkCharacterCount = 0;

        for (auto c : text)
        {
            if (isalnum(c))
            {
                length++;
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
        restLength = length;
    }

    bool initWord(const AnalyzedWord& anagram, const std::string& text)
    {
        memset(&counts, 0, sizeof(counts));
        memcpy(&remaining, anagram.remaining, sizeof(remaining));
        remainingMask = anagram.usedMask;
        usedMask = 0;
        checkCharacterCount = anagram.checkCharacterCount;
        memcpy(checkCharacters, anagram.checkCharacters, sizeof(checkCharacters));
        length = (int)text.size();
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
