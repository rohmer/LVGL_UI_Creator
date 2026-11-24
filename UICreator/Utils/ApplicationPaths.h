#pragma once
#include "ExecutablePaths.h"
#ifdef __APPLE__
#include <SDL3/SDL_filesystem.h>
#endif

namespace ApplicationPaths {
inline std::filesystem::path ResourceDirectory()
{
#ifdef __APPLE__
    // SDL resolves a .app's Contents/Resources directory, and the executable
    // directory for an unbundled development build. No working-directory guess.
    if (const auto* base = SDL_GetBasePath(); base && *base) return std::filesystem::u8path(base);
#endif
    return ExecutableDirectory();
}
}
