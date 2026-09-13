#include "game/GameLock.h"

namespace d2bs::game {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables) - thread-local by design
thread_local std::shared_lock<std::shared_mutex> GameReadLock::lock_;
thread_local std::unique_ptr<GameWriteLock> GameWriteLock::manual_;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace d2bs::game
