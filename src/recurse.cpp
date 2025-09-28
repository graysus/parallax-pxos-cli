
#include <cerrno>
#include <filesystem>
#include <functional>
#include <string>
#include <sys/stat.h>
#include <PxResult.hpp>
#include <PxState.hpp>
#include <PxFunction.hpp>
#include <unistd.h>
#include <recurse.hpp>

static inline PxResult::Result<void> pxchown(std::string path, uid_t uid, gid_t gid) {
    return PxFunction::wrap("chown", chown(path.c_str(), uid, gid));
}

static inline PxResult::Result<void> pxchmod(std::string path, mode_t mode) {
    return PxFunction::wrap("chmod", chmod(path.c_str(), mode));
}

PxResult::Result<void> fcopy(std::string pathin, std::string pathout, struct stat& st) {
    if (S_ISDIR(st.st_mode)) {
        // not recursive copy, so just make the directory and give it the same mods.
        auto md_res = PxFunction::wrap("mkdir", mkdir(pathout.c_str(), st.st_mode));
        if (md_res.eno != EEXIST) PXASSERTM(md_res, "fcopy");
        PXASSERTM(pxchown(pathout, st.st_uid, st.st_gid), "fcopy");
    }
    if (S_ISREG(st.st_mode)) {
        // TODO: proper copy function
        auto read_contentres = PxState::fget(pathin);
        PXASSERTM(read_contentres, "fcopy");
        PxState::fput(pathout, read_contentres.assert());

        PXASSERTM(pxchown(pathout, st.st_uid, st.st_gid), "fcopy");
        PXASSERTM(pxchmod(pathout, st.st_mode), "fcopy");
    }
    if (S_ISLNK(st.st_mode)) {
        char buf[4096];
        PXASSERTM(PxFunction::wrap("readlink", readlink(pathin.c_str(), buf, 4096)), "fcopy");
        symlink(buf, pathout.c_str());
        // no chown or chmod, since symlinks don't have permissions
    }
    return PxResult::Null;
}

PxResult::Result<void> fsrecurse(std::string path, std::string pathrel, fsrhnd_t fenter, fsrhnd_t fexit) {
    struct stat st;
    lstat(path.c_str(), &st);

    PXASSERT(fenter(path, pathrel, st));

    if (!S_ISDIR(st.st_mode)) {
        return fexit(path, pathrel, st);
    }

    for (auto i : std::filesystem::directory_iterator(path)) {
        if (!pathrel.empty() && !PxFunction::endsWith(pathrel, "/")) {
            pathrel += "/";
        }
        PXASSERT(fsrecurse(i.path(), pathrel + i.path().filename().string(), fenter, fexit));
    }

    return fexit(path, pathrel, st);
}

PxResult::Result<void> mergedir(std::string to, std::string from, bool replace) {
    return fsrecurse(from, "", [from, to, replace](auto _, auto rel, auto st) -> PxResult::Result<void> {
        struct stat st2;
        int err = lstat((to+"/"+rel).c_str(), &st2);
        
        if (err != 0 && errno != ENOENT) {
            return PxResult::FResult("mergedir / lstat", errno);
        }
        if (err != 0 && errno == ENOENT) {
            // do nothing
        } else if (S_ISDIR(st2.st_mode) || !replace) {
            if (!S_ISLNK(st2.st_mode)) {
                PXASSERTM(PxFunction::wrap("chown", chown((to+"/"+rel).c_str(), st.st_uid, st.st_gid)), "mergedir");
                PXASSERTM(PxFunction::wrap("chmod", chmod((to+"/"+rel).c_str(), st.st_mode)), "mergedir");
            }
            return PxResult::Null;
        } else {
            PXASSERTM(PxFunction::wrap("remove", remove((to+"/"+rel).c_str())), "mergedir");
        }

        return fcopy(from+"/"+rel, to+"/"+rel, st);
    }, FHND_NONE);
}

PxResult::Result<void> removerecursedir(std::string dir) {
    return fsrecurse(dir, "", FHND_NONE, [](auto path, auto _, auto st) -> PxResult::Result<void> {
        int err;
        if (S_ISDIR(st.st_mode)) {
            err = rmdir(path.c_str());
        } else {
            err = remove(path.c_str());
        }
        if (err) return PxResult::FResult("removerecursedir / remove", errno);
        return PxResult::Null;
    });
}