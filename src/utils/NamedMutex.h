#pragma once

#include <Windows.h>

#include <cstdint>

namespace d2bs::utils {

// RAII wrapper over a Win32 named mutex, used as a cross-process advisory lock.
// Construction creates-or-opens the named object and waits for ownership;
// destruction releases and closes it. The name selects the lock, so cooperating
// processes (and other languages - e.g. a .NET System.Threading.Mutex of the
// same name) serialize against each other.
//
// Check Acquired() before relying on the lock: it is false only on timeout, and
// the caller may still choose to proceed - any resource this guards should be
// committed atomically regardless, so the worst case is a lost update under
// pathological contention, never a corrupt file.
//
// A mutex abandoned by a crashed holder (WAIT_ABANDONED) is treated as acquired:
// the OS hands us ownership, and an atomically-committed resource cannot have
// been left torn by the dead writer.
class NamedMutexLock {
   public:
    static constexpr uint32_t DEFAULT_TIMEOUT_MS = 5000;

    explicit NamedMutexLock(const wchar_t* name, uint32_t timeoutMs = DEFAULT_TIMEOUT_MS)
        : handle_(CreateMutexW(nullptr, FALSE, name)) {
        if (handle_ != nullptr) {
            const DWORD result = WaitForSingleObject(handle_, timeoutMs);
            acquired_ = (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED);
        }
    }

    ~NamedMutexLock() {
        if (handle_ != nullptr) {
            if (acquired_) {
                ReleaseMutex(handle_);
            }
            CloseHandle(handle_);
        }
    }

    NamedMutexLock(const NamedMutexLock&) = delete;
    NamedMutexLock& operator=(const NamedMutexLock&) = delete;
    NamedMutexLock(NamedMutexLock&&) = delete;
    NamedMutexLock& operator=(NamedMutexLock&&) = delete;

    bool Acquired() const { return acquired_; }

   private:
    HANDLE handle_ = nullptr;
    bool acquired_ = false;
};

}  // namespace d2bs::utils
