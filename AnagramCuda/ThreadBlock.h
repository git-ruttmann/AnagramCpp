#pragma once

#include <mutex>
#include <thread>
#include <vector>

#include "AnalyzedWord.h"
#include "PartialAnagram.h"

class ThreadBlock
{
public:
    ThreadBlock();
    ThreadBlock(ThreadBlock&& other) noexcept;
    ~ThreadBlock();

    /// <summary>
    /// combining the search word with every entry in the data block. Run in a separate thread.
    /// </summary>
    /// <param name="word">the search word</param>
    /// <param name="data">the data block. MUST NOT MODIFY until WaitForResult returns</param>
    void ScanBlockInThread(const std::vector< std::vector<partialAnagramEntry>>& data);

    void WaitForResult();

    void TerminateThread();

    void CombineBlock(const std::vector<partialAnagramEntry>& data, size_t startOffset);

    /// <summary>
    /// new partial combinations for the current word
    /// </summary>
    std::vector<partialAnagramEntry> m_generatedEntries;

    /// <summary>
    /// indizes in data that form a valid result together with the currently scanned word
    /// </summary>
    std::vector<int> m_resultWordLength;
    std::vector<int> m_resultWordId;

    AnalyzedWord m_word;

    int perfcount1;
    int perfcount2;
    int perfcount3;

private:
    void RunThread();
    void ProcessAllBlocks();
    void ProcessList(const AnalyzedWord& word, const std::vector<partialAnagramEntry>& list, size_t start);

    const std::vector<std::vector<partialAnagramEntry>>* m_data;
    bool m_dataReady;
    bool m_dataCompleted;
    const int m_minimumWordLength;

    std::mutex m_lock;
    std::condition_variable m_dataReadyCondition;
    std::condition_variable m_dataCompletedCondition;
    bool m_finishThread;
    std::thread m_thread;
};

