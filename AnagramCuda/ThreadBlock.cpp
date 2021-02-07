#include "ThreadBlock.h"

ThreadBlock::ThreadBlock(const SOptions& options)
    : m_options(options)
{
    m_data = NULL;
    m_finishThread = false;
    m_dataReady = false;
    m_dataCompleted = false;
    m_thread = std::thread([this] { this->RunThread(); });

    perfcount1 = 0;
    perfcount2 = 0;
    perfcount3 = 0;
}

ThreadBlock::~ThreadBlock() noexcept
{
    {
        std::lock_guard guard(m_lock);
        m_data = NULL;
        m_finishThread = true;
        m_dataReadyCondition.notify_one();
    }

    m_thread.join();
}

ThreadBlock::ThreadBlock(ThreadBlock&& other) noexcept
    : m_options(other.m_options)
{
    m_data = NULL;
    m_finishThread = false;
    m_dataReady = false;
    m_dataCompleted = false;
    m_thread = std::thread([this] { this->RunThread(); });

    other.m_finishThread = true;

    perfcount1 = 0;
    perfcount2 = 0;
    perfcount3 = 0;
}

void ThreadBlock::RunThread()
{
    while (!m_finishThread)
    {
        {
            std::unique_lock<std::mutex> uniqueLock(m_lock);
            m_dataReadyCondition.wait(uniqueLock, [&] { return m_finishThread || m_dataReady; });
            m_dataReady = false;
        }

        if (!m_finishThread)
        {
            ProcessAllBlocks();
        }

        {
            std::lock_guard guard(m_lock);
            m_dataCompleted = true;
            m_dataCompletedCondition.notify_one();
        }
    }
}

void ThreadBlock::ScanBlockInThread(const std::vector<std::vector<partialAnagramEntry>>& data)
{
    std::lock_guard guard(m_lock);
    m_data = &data;
    m_dataReady = true;
    m_dataCompleted = false;
    m_dataReadyCondition.notify_one();
}

void ThreadBlock::WaitForResult()
{
    std::unique_lock<std::mutex> uniqueLock(m_lock);
    m_dataCompletedCondition.wait(uniqueLock, [&] { return m_dataCompleted; });
}

void ThreadBlock::TerminateThread()
{
    {
        std::lock_guard guard(m_lock);
        m_data = NULL;
        m_finishThread = true;
        m_dataReadyCondition.notify_one();
    }

    WaitForResult();
}

void ThreadBlock::CombineBlock(const std::vector<partialAnagramEntry>& data, size_t startOffset)
{
    ProcessList(m_word, data, startOffset);
}

inline bool EntryIsComplete(const partialAnagramEntry& entry, const AnalyzedWord& word)
{
    for (size_t i = 0; i < possibleCharacterCount; i++)
    {
        if (entry.counts[i] != word.counts[i])
        {
            return false;
        }
    }

    return true;
}

inline bool EntryIsPart(const partialAnagramEntry& entry, const AnalyzedWord& word)
{
    for (size_t i = 0; i < possibleCharacterCount; i++)
    {
        if (entry.counts[i] < word.counts[i])
        {
            return false;
        }
    }

    return true;
}

void ThreadBlock::ProcessList(const AnalyzedWord& word, const std::vector<partialAnagramEntry>& list, size_t start)
{
    const auto listSize = list.size();
    if (listSize <= start)
    {
        return;
    }

    auto restLength = list.front().restLength;
    if (restLength == word.length)
    {
        for (size_t i = start; i < listSize; i++)
        {
//            perfcount1++;
            const auto& entry = list[i];
            if (entry.doNotUseMask & word.usedMask)
            {
                continue;
            }

            perfcount2++;
            if (EntryIsComplete(entry, word))
            {
                m_resultWordId.emplace_back((int)i);
                m_resultWordLength.emplace_back(restLength);
            }
        }
    }
    else if (restLength >= word.length + m_options.MinWordLength)
    {
        for (size_t i = start; i < listSize; i++)
        {
            perfcount1++;
            const auto& entry = list[i];
            if (entry.doNotUseMask & word.usedMask)
            {
                continue;
            }

            perfcount3++;
            if (EntryIsPart(entry, word))
            {
                m_generatedEntries.resize(m_generatedEntries.size() + 1);
                m_generatedEntries.back().joinWord(entry, word, (int)i);
            }
        }
    }
}

void ThreadBlock::ProcessAllBlocks()
{
    const auto& word = m_word;
    const auto anagramLength = m_data->size();

    for (int32_t restLength = m_options.MinWordLength; restLength < anagramLength; restLength++)
    {
        const auto& dataWithLength = m_data->at(restLength);
        ProcessList(m_word, m_data->at(restLength), 0);
    }
}
