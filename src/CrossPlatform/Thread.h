// TortoiseGit - a Windows shell extension for easy version control
// Copyright (C) 2025 - TortoiseGit
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
#pragma once

#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include <chrono>
#include <vector>
#include <queue>
#include <cassert>

// DWORD type alias for API compatibility
using DWORD = unsigned long;
constexpr DWORD INFINITE = 0xFFFFFFFF;

// Cross-platform critical section / mutex
// Replaces MFC CCriticalSection / ATL CCritSec
class CCriticalSection
{
public:
    CCriticalSection() = default;
    ~CCriticalSection() = default;
    CCriticalSection(const CCriticalSection&) = delete;
    CCriticalSection& operator=(const CCriticalSection&) = delete;

    void Lock()   { m_mutex.lock(); }
    void Unlock() { m_mutex.unlock(); }

    class CAutoLock
    {
    public:
        explicit CAutoLock(CCriticalSection& cs) : m_cs(cs) { m_cs.Lock(); }
        ~CAutoLock() { m_cs.Unlock(); }
        CAutoLock(const CAutoLock&) = delete;
        CAutoLock& operator=(const CAutoLock&) = delete;
    private:
        CCriticalSection& m_cs;
    };

private:
    std::mutex m_mutex;
};

// Reader-writer lock
// Replaces MFC CMultiReadExclusiveWriteSem
class CReaderWriterLock
{
public:
    CReaderWriterLock() = default;
    ~CReaderWriterLock() = default;

    void LockRead()  { m_rwlock.lock_shared(); }
    void UnlockRead() { m_rwlock.unlock_shared(); }
    void LockWrite() { m_rwlock.lock(); }
    void UnlockWrite() { m_rwlock.unlock(); }

    class CAutoReadLock
    {
    public:
        explicit CAutoReadLock(CReaderWriterLock& rwl) : m_rwl(rwl) { m_rwl.LockRead(); }
        ~CAutoReadLock() { m_rwl.UnlockRead(); }
    private:
        CReaderWriterLock& m_rwl;
    };

    class CAutoWriteLock
    {
    public:
        explicit CAutoWriteLock(CReaderWriterLock& rwl) : m_rwl(rwl) { m_rwl.LockWrite(); }
        ~CAutoWriteLock() { m_rwl.UnlockWrite(); }
    private:
        CReaderWriterLock& m_rwl;
    };

private:
    std::shared_mutex m_rwlock;
};

// Event / semaphore - synchronization primitive
// Replaces MFC CEvent / CSemaphore
class CEvent
{
public:
    enum AutoReset { Auto, Manual };
    enum InitialState { Signaled, NotSignaled };

    CEvent(InitialState state = NotSignaled, AutoReset type = Auto);
    ~CEvent() = default;

    void Set();
    void Reset();
    bool Wait(DWORD dwMilliseconds = INFINITE);
    bool IsSignaled() const;

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_signaled;
    bool m_autoReset;
};

// Thread class
// Replaces MFC AfxBeginThread / _beginthreadex
class CThread
{
public:
    using ThreadFunc = std::function<int(void*)>;
    using ThreadHandle = std::thread;

    CThread() : m_thread(), m_handle(nullptr) {}
    virtual ~CThread();

    CThread(const CThread&) = delete;
    CThread& operator=(const CThread&) = delete;

    // Start a thread. Returns false on failure.
    bool Create(ThreadFunc func, void* pData = nullptr,
                DWORD dwCreateFlags = 0, DWORD dwStackSize = 0);

    // Wait for thread to finish
    void WaitForFinish(DWORD dwMilliseconds = INFINITE);

    // Check if thread is still running
    bool IsRunning() const;

    // Get thread ID
    DWORD GetThreadId() const;

    // Terminate thread forcefully
    void Terminate(DWORD dwExitCode = 0);

    // Thread function
    int Run();

private:
#ifdef _WIN32
    static unsigned int __stdcall ThreadProc(void* pData);
#else
    static unsigned int ThreadProc(void* pData);
#endif
    int InternalRun(ThreadFunc func, void* pData);

    ThreadFunc m_func;
    void* m_data;
    std::thread m_thread;
    bool m_running;
    void* m_handle = nullptr;
};

// Waitable event / condition variable wrapper
class CWaitableEvent
{
public:
    CWaitableEvent(bool initialState = false);
    ~CWaitableEvent() = default;

    void Set();
    void Reset();
    bool Wait(DWORD dwMilliseconds = INFINITE);

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_signaled;
};

// Thread-local storage helper
template<typename T>
class CThreadLocal
{
public:
    CThreadLocal() = default;
    ~CThreadLocal() = default;

    T& Get() { return m_tls; }
    void Set(const T& value) { m_tls = value; }

private:
    thread_local T m_tls;
};

// Inline functions
inline CEvent::CEvent(InitialState state, AutoReset type)
    : m_signaled(state == Signaled), m_autoReset(type == Auto)
{
}

inline void CEvent::Set()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_signaled = true;
    }
    m_cv.notify_all();
}

inline void CEvent::Reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_signaled = false;
}

inline bool CEvent::Wait(DWORD dwMilliseconds)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_autoReset)
    {
        // Auto-reset: wait until signaled, then auto-reset
        m_cv.wait(lock, [this]{ return m_signaled; });
        m_signaled = false;
        return true;
    }
    else
    {
        // Manual-reset: wait until signaled
        if (dwMilliseconds == INFINITE)
        {
            m_cv.wait(lock, [this]{ return m_signaled; });
        }
        else
        {
            auto ret = m_cv.wait_for(lock, std::chrono::milliseconds(dwMilliseconds),
                                     [this]{ return m_signaled; });
            return ret;
        }
        return true;
    }
}

inline bool CEvent::IsSignaled() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_signaled;
}

inline CWaitableEvent::CWaitableEvent(bool initialState)
    : m_signaled(initialState)
{
}

inline void CWaitableEvent::Set()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_signaled = true;
    }
    m_cv.notify_all();
}

inline void CWaitableEvent::Reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_signaled = false;
}

inline bool CWaitableEvent::Wait(DWORD dwMilliseconds)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (dwMilliseconds == INFINITE)
    {
        m_cv.wait(lock, [this]{ return m_signaled; });
    }
    else
    {
        auto ret = m_cv.wait_for(lock, std::chrono::milliseconds(dwMilliseconds),
                                 [this]{ return m_signaled; });
        return ret;
    }
    return true;
}

// Simple thread pool for job scheduling
class CThreadPool
{
public:
    static CThreadPool& Instance();
    void PostTask(std::function<void()> task);
    void PostTaskDelayed(std::function<void()> task, DWORD milliseconds);
    void Shutdown();

private:
    CThreadPool(int numThreads = 4);
    ~CThreadPool();

    void WorkerFunc();

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_stop{false};
};
