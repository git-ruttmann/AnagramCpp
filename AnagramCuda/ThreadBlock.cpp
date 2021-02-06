#include "ThreadBlock.h"

ThreadBlock::ThreadBlock()
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

ThreadBlock::ThreadBlock(ThreadBlock&& other) noexcept
{
    m_data = NULL;
    m_finishThread = false;
    m_dataReady = false;
    m_dataCompleted = false;
    m_thread = std::move(other.m_thread);

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
            ProcessBlock(0);
        }

        {
            std::lock_guard guard(m_lock);
            m_dataCompleted = true;
            m_dataCompletedCondition.notify_one();
        }
    }
}

void ThreadBlock::ScanBlockInThread(const std::vector<partialAnagramEntry>& data)
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
    m_data = &data;
    ProcessBlock(startOffset);
}

void ThreadBlock::ProcessBlock(size_t startOffset)
{
    const auto& word = m_word;
    auto arraySize = m_data->size();
    for (size_t i = startOffset; i < arraySize; i++)
    {
        perfcount1++;
        const auto& entry = (*m_data)[i];
        if (entry.restLength < word.length)
        {
            perfcount2++;
            continue;
        }

        if (entry.doNotUseMask & word.usedMask)
        {
            perfcount3++;
            continue;
        }

        usedCharacterCount_t maskOfSum = 0;
        for (size_t j = 0; j < possibleCharacterCount; j++)
        {
            maskOfSum |= (entry.counts[j] - word.counts[j]);
        }

        if (maskOfSum == 0)
        {
            m_results.emplace_back((int)i);
        }
        else if ((maskOfSum > 0) && (entry.restLength - word.length > 2))
        {
            m_generatedEntries.resize(m_generatedEntries.size() + 1);
            m_generatedEntries.back().joinWord(entry, word, (int)i);
        }
    }
}
