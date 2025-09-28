
#ifndef PXOS_RECURSE
#define PXOS_RECURSE

#include <functional>
#include <string>
#include <PxResult.hpp>

typedef std::function<PxResult::Result<void>(std::string path, std::string relpath, struct stat& st)> fsrhnd_t;
PxResult::Result<void> fcopy(std::string pathin, std::string pathout, struct stat& st);
PxResult::Result<void> fsrecurse(std::string path, std::string pathrel, fsrhnd_t fenter, fsrhnd_t fexit);
PxResult::Result<void> mergedir(std::string to, std::string from, bool replace);
PxResult::Result<void> removerecursedir(std::string dir);

#define FHND_NONE (fsrhnd_t)[](auto _, auto _2, auto _3) { return PxResult::Null; }

#endif