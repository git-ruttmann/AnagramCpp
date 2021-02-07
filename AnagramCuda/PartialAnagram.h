#pragma once

#include "AnalyzedWord.h"

struct PartialAnagram
{
    PartialAnagram()
    {
        doNotUseMask = 0;
        previousEntry = -1;
        previousLength = 0;
        wordId = 0;
        restLength = 0;
        memset(remaining, 0, sizeof(remaining));
    }

    PartialAnagram(const PartialAnagram& entry, const AnalyzedWord& word, int previous)
    {
        joinWord(entry, word, previous);
    }

    /// <summary>
    /// Constructor from a singleWord
    /// </summary>
    /// <param name="word"></param>
    PartialAnagram(const AnalyzedWord& word)
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
    /// The length container of the previous entry
    /// </summary>
    int32_t previousLength;

    /// <summary>
    /// The id of the word in a global array
    /// </summary>
    int32_t wordId;

    /// <summary>
    /// the counts per character
    /// </summary>
    usedCharacterCount_t remaining[possibleCharacterCount];

    /// <summary>
    /// number of characters missing until anagram is complete
    /// </summary>
    int32_t restLength;

private:
    /// <summary>
    /// copy data from an analysed word
    /// </summary>
    /// <param name="word">the word</param>
    void initFromAnalyzedWord(const AnalyzedWord& word)
    {
        memcpy(&remaining, word.remaining, sizeof(remaining));
        doNotUseMask = ~word.remainingMask;
        wordId = word.wordId;
        restLength = word.restLength;
        previousEntry = -1;
    }

    /// <summary>
    /// join existing data with a new word
    /// </summary>
    void joinWord(const PartialAnagram& entry, const AnalyzedWord& word, int previous)
    {
        previousEntry = previous;
        previousLength = entry.restLength;

        doNotUseMask = entry.doNotUseMask;
        wordId = word.wordId;
        restLength = entry.restLength - word.length;

        for (int i = 0; i < word.numberOfCharacterClasses; i++)
        {
            auto count = entry.remaining[i] - word.counts[i];
            remaining[i] = count;
            if (count == 0)
            {
                doNotUseMask |= i < 32 ? (1 << i) : (1 << 31);
            }
        }
    }
};

