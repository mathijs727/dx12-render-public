#include "Engine/Util/DirectoryChangeWatcher.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <Windows.h>
#include <spdlog/spdlog.h>
DISABLE_WARNINGS_POP()
#include <cstddef>
#include <tbx/error_handling.h>
#include <vector>

namespace Util {

struct DirectoryChangeWatcher::Implementation {
    HANDLE directoryHandle;
    std::vector<std::byte> buffer;
    std::unique_ptr<OVERLAPPED> pOverlapped;
};

DirectoryChangeWatcher::DirectoryChangeWatcher(const std::filesystem::path& directoryPath, std::chrono::milliseconds delay)
    : m_delay(delay)
{
    // https://gist.github.com/nickav/a57009d4fcc3b527ed0f5c9cf30618f8
    // https://learn.microsoft.com/en-us/answers/questions/1021759/using-readdirectorychangesw()-to-get-information-a
    m_pImpl = std::make_unique<Implementation>();
    m_pImpl->directoryHandle = CreateFile(
        directoryPath.string().c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL);
    Tbx::assert_always(m_pImpl->directoryHandle != INVALID_HANDLE_VALUE);

    m_pImpl->buffer.resize(10000);
    setReadDirectoryChange();
}

DirectoryChangeWatcher::~DirectoryChangeWatcher()
{
    CloseHandle(m_pImpl->directoryHandle);
    CloseHandle(m_pImpl->pOverlapped->hEvent);
}

bool DirectoryChangeWatcher::hasChanged()
{
    DWORD bytesTransferred = 0;
    if (GetOverlappedResult(m_pImpl->directoryHandle, m_pImpl->pOverlapped.get(), &bytesTransferred, false)) {
        DWORD offset = 0;
        while (true) {
            auto pFileNotifyInformation = (FILE_NOTIFY_INFORMATION const*)&m_pImpl->buffer[offset];
            offset += pFileNotifyInformation->NextEntryOffset;
            if (pFileNotifyInformation->NextEntryOffset == 0)
                break;
        }

        CloseHandle(m_pImpl->pOverlapped->hEvent);
        setReadDirectoryChange();
        m_timerRunning = true;
        m_lastChange = clock::now();
    }

    if (!m_timerRunning)
        return false;
    const auto timeSinceLastChange = clock::now() - m_lastChange;
    if (timeSinceLastChange < m_delay)
        return false;
    m_timerRunning = false;
    return true;
}

void DirectoryChangeWatcher::setReadDirectoryChange()
{
    // Asynchronously signals the event if a file changes.
    m_pImpl->pOverlapped = std::make_unique<OVERLAPPED>();
    m_pImpl->pOverlapped->hEvent = CreateEvent(NULL, FALSE /* manual reset */, FALSE /* initial state */, NULL);

    DWORD bytesReturned = 0;
    BOOL resNdc = ReadDirectoryChangesW(
        m_pImpl->directoryHandle,
        m_pImpl->buffer.data(), (DWORD)m_pImpl->buffer.size(),
        TRUE, // watch subtree.
        FILE_NOTIFY_CHANGE_LAST_WRITE,
        &bytesReturned,
        m_pImpl->pOverlapped.get(), // Overlapped
        NULL); // Coroutine
    Tbx::assert_always(resNdc != 0);
}

}