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

    /// <summary>
    /// combining the search word with every entry in the data block. Run in a separate thread.
    /// </summary>
    /// <param name="word">the search word</param>
    /// <param name="data">the data block. MUST NOT MODIFY until WaitForResult returns</param>
    void ScanBlockInThread(const std::vector<partialAnagramEntry>& data);

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
    std::vector<int> m_results;

    AnalyzedWord m_word;

private:
    void RunThread();
    void ProcessBlock(size_t startOffset);

    const std::vector<partialAnagramEntry>* m_data;
    bool m_dataReady;
    bool m_dataCompleted;

    std::mutex m_lock;
    std::condition_variable m_dataReadyCondition;
    std::condition_variable m_dataCompletedCondition;
    bool m_finishThread;
    std::thread m_thread;
};

