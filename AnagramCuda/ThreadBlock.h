#pragma once

#include <mutex>
#include <thread>
#include <vector>

#include "Options.h"
#include "AnalyzedWord.h"
#include "PartialAnagram.h"

class ThreadBlock
{
public:
    ThreadBlock(const SOptions& options, const std::vector<std::vector<partialAnagramEntry>>& data);
    ThreadBlock(ThreadBlock&& other) noexcept;
    ~ThreadBlock();

    /// <summary>
    /// combining the search word with every entry in the data block. Run in a separate thread.
    /// </summary>
    void ScanBlockInThread();

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
    std::vector<std::vector<int>> m_results;

    AnalyzedWord m_word;

    int perfcount1;
    int perfcount2;
    int perfcount3;

private:
    void RunThread();
    void ProcessAllBlocks();
    void ProcessList(const AnalyzedWord& word, const std::vector<partialAnagramEntry>& list, size_t start);

    void AddResult(int wordId, const partialAnagramEntry & entry);

    const std::vector<std::vector<partialAnagramEntry>>& m_data;
    bool m_dataReady;
    bool m_dataCompleted;

    const SOptions& m_options;

    std::mutex m_lock;
    std::condition_variable m_dataReadyCondition;
    std::condition_variable m_dataCompletedCondition;
    bool m_finishThread;
    std::thread m_thread;
};

