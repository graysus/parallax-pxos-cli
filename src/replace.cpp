
#include <string>
#include <PxResult.hpp>
#include <PxOSConfig.hpp>
#include <filesystem>
#include <PxMount.hpp>
#include <PxDefer.hpp>
#include <recurse.hpp>
#include <sys/stat.h>
#include <unistd.h>

PxResult::Result<void> replace(std::string replace_with) {
    PxLog::log.info("Initializing new system...");
    
    PxOSConfig::conf c("/data/partitions");
    PXASSERT(c.readConf());

    PXASSERT(PxOSConfig::InitializeNew(c));

    // save it now, since the previous operation cannot be undone
    PXASSERT(c.writeConf());

    if (!std::filesystem::is_directory("/mnt/.px-second")) {
        std::error_code ec;
        std::filesystem::create_directory("/mnt/.px-second", ec);
        if (ec) return PxResult::FResult("std::filesystem::create_directory", ec.value());
    }

    std::cout << c.oppositePart() << "\n";
    PXASSERTM(PxMount::Mount(c.oppositePart(), "/mnt/.px-second"), "mount second");
    DEFER_RV(umount_second, {
        PxLog::log.info("Cleaning up...");
        if (system("umount -R /mnt/.px-second") != 0) {
            return PxResult::FResult("failure", EINVAL);
        }
    });

    PxLog::log.info("Installing image to new system...");

    if (system(("tar xpf "+replace_with+" --xattrs-include=\\* -C /mnt/.px-second").c_str()) != 0) {
        return PxResult::FResult("system", EINVAL);
    }

    PXASSERTM(mergedir("/boot", "/mnt/.px-second/boot.def", true), "replace");

    // TODO: overwrite files with no changes made
    PXASSERTM(mergedir("/etc", "/mnt/.px-second/etc.def", false), "replace");
    PXASSERTM(mergedir("/var", "/mnt/.px-second/var.def", false), "replace");

    PXASSERTM(removerecursedir("/mnt/.px-second/etc.def"), "replace");
    PXASSERTM(removerecursedir("/mnt/.px-second/var.def"), "replace");
    PXASSERTM(removerecursedir("/mnt/.px-second/boot.def"), "replace");

    for (auto &i : {"run", "tmp", "proc", "sys", "dev", "data", "boot", "var", "etc"}) {
        auto newpath = "/mnt/.px-second/"+(std::string)i;
        if (!std::filesystem::is_directory(newpath)) {
            std::error_code ec;
            std::filesystem::create_directory(newpath, ec);
            if (ec) return PxResult::FResult("std::filesystem::create_directory", ec.value());
        }
    }

    // TODO: unmount individually instead of using recursive unmount
    // also probably uhhhh... maybe a little more data driven would be nice?
    PXASSERTM(PxMount::SimpleMount("tmpfs", "/mnt/.px-second/run", "tmpfs"), "mount run");
    PXASSERTM(PxMount::SimpleMount("tmpfs", "/mnt/.px-second/tmp", "tmpfs"), "mount tmp");
    PXASSERTM(PxMount::SimpleMount("proc", "/mnt/.px-second/proc", "proc"), "mount proc");
    PXASSERTM(PxMount::SimpleMount("sysfs", "/mnt/.px-second/sys", "sysfs"), "mount sys");
    PXASSERTM(PxMount::SimpleMount("devtmpfs", "/mnt/.px-second/dev", "devtmpfs"), "mount dev");
    PXASSERTM(PxMount::Mount("/boot", "/mnt/.px-second/boot", "", "bind"), "mount boot");
    PXASSERTM(PxMount::Mount("/root", "/mnt/.px-second/root", "", "bind"), "mount root");
    PXASSERTM(PxMount::Mount("/var", "/mnt/.px-second/var", "", "bind"), "mount var");
    PXASSERTM(PxMount::Mount("/etc", "/mnt/.px-second/etc", "", "bind"), "mount etc");
    PXASSERTM(PxMount::Mount(c.data, "/mnt/.px-second/data"), "mount data");

    c.switchCurrent();
    PXASSERT(c.writeConf());
    DEFER_RV(switch_back, {
        c.switchCurrent();
        PXASSERT(c.writeConf());
    });

    PxLog::log.info("Generating boot files...");
    if (system("chroot /mnt/.px-second sh -c 'mkinitcpio -P >/dev/null && grub-mkconfig -o /boot/grub/grub.cfg >/dev/null'") != 0){
        return PxResult::FResult("system", EINVAL);
    }

    PXASSERT(switch_back.finish());
    PXASSERT(umount_second.finish());

    auto cmd = "sed 's\1" + c.curPart() + "\1" + c.oppositePart() + "\1' /etc/fstab -i";
    if (system(cmd.c_str()) != 0) {
        return PxResult::FResult("system sed", EINVAL);
    }

    c.current = c.current == "1" ? "2" : "1";
    PXASSERT(c.writeConf());

    return PxResult::Null;
}