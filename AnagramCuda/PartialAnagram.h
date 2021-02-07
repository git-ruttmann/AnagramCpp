#pragma once

#include "AnalyzedWord.h"

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
        restLength = word.restLength;
        previousEntry = -1;
    }

    /// <summary>
    /// join existing data with a new word
    /// </summary>
    void joinWord(const partialAnagramEntry& entry, const AnalyzedWord& word, int previous)
    {
        previousEntry = previous;
        previousLength = entry.restLength;

        doNotUseMask = entry.doNotUseMask;
        wordId = word.wordId;
        restLength = entry.restLength - word.length;

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

