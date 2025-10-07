#include <PxOSConfig.hpp>
#include <PxDefer.hpp>
#include <PxMount.hpp>
#include <cerrno>
#include <blkid/blkid.h>

namespace PxOSConfig {
    PxResult::Result<void> InitializeNew(conf &cfg) {
        // Resolve the current opposite
        auto opposite = mnt_resolve_spec(cfg.oppositePart().c_str(), NULL);
        if (opposite == NULL)
            return PxResult::FResult("PxOSConfig::SwitchPart (no such partition)", ENODEV);

        DEFER(free_opposite, free(opposite));

        // Create a new filesystem
        std::string cmd = "mkfs.ext4 -qF " + (std::string)opposite;
        if (system(cmd.c_str()) != 0) {
            return PxResult::FResult("PxOSConfig::InitializeNew / mkfs", EINVAL);
        }

        // Get the UUID of the new partition and store it.

        std::string uuid;
        {
            const char* uuidchr;
            size_t uuidlen;

            blkid_probe probe = blkid_new_probe_from_filename(opposite);
            if (probe==NULL)
                return PxResult::FResult("PxOSConfig::SwitchPart / blkid_new_probe_from_filename", errno);

            DEFER(free_probe, blkid_free_probe(probe));

            blkid_do_probe(probe);
            blkid_probe_lookup_value(probe, "UUID", &uuidchr, &uuidlen);

            uuid = std::string(uuidchr, uuidlen-1);
        }

        std::string saveID = opposite;

        if (uuid.length() > 0) {
            saveID = "UUID="+uuid;
        }
        cfg.oppositePart() = saveID;

        return PxResult::Null;
    }
}