#include <PxDefer.hpp>
#include <PxArg.hpp>
#include <PxMount.hpp>
#include <PxLog.hpp>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <libmount/libmount.h>
#include <string>
#include <unistd.h>
#include <PxOSConfig.hpp>
#include <replace.hpp>
#include <vector>

typedef PxResult::Result<void>(*action_t)(std::vector<std::string> &additionalArgs);

PxOSConfig::OSConfig osconf;

struct command_t {
    std::string name;
    std::string help;
    bool needsRoot;
    action_t action;
};

PxResult::Result<bool> CheckUpdates(std::string &version, std::string &old_version) {
    auto pxos_curversionres = PxState::fget("/lib/parallaxos-version");
    PXASSERTM(pxos_curversionres, "CheckUpdates");
    old_version = pxos_curversionres.assert();
    old_version = PxFunction::trim(old_version);

    DEFER(verfile, {
        if (std::filesystem::exists("/var/tmp/newver")) {
            remove("/var/tmp/newver");
        }
    });

    if (system(("curl -so /var/tmp/newver "+osconf.repo+"/"+osconf.branch).c_str()) != 0) {
        return PxResult::FResult("CheckUpdates / system", EINVAL);
    }

    auto pxos_newversionres = PxState::fget("/var/tmp/newver");
    PXASSERTM(pxos_newversionres, "CheckUpdates");
    version = pxos_newversionres.assert();
    version = PxFunction::trim(version);
    
    return version != old_version;
}

PxResult::Result<void> clear_fetch_files(const std::vector<std::string> &keep) {
    if (std::filesystem::exists("/var/tmp/px-dl")) {
        for (auto i : std::filesystem::directory_iterator("/var/tmp/px-dl")) {
            if (PxFunction::contains(keep, i.path().filename())) continue;
            PXASSERTM(PxFunction::wrap("remove", remove(i.path().c_str())), "clear_fetch_files");
        }
    }
    return PxResult::Null;
}

PxResult::Result<void> cmd_update(std::vector<std::string> &extra_args) {
    if (geteuid() != 0) {
        PxLog::log.error("Must be root!");
        exit(1);
    }
    std::string old_version, version;
    PxResult::Result<bool> upd = CheckUpdates(version, old_version);

    bool shouldUpdate = upd.assert();

    if (shouldUpdate) {
        std::cout << "\x1b[1mA new version is available (" << old_version << " -> " << version << "). Update? [Y/n] \x1b[0m";

        bool isValid = false;
        bool hasConfirmed = false;
        std::string userConfirm;
        do {
            std::getline(std::cin, userConfirm);
            userConfirm = PxFunction::trim(userConfirm);
            isValid = PxFunction::contains({"", "y", "n", "Y", "N"}, userConfirm);
            hasConfirmed = PxFunction::contains({"", "y", "Y"}, userConfirm);
        } while (!isValid);


        if (!hasConfirmed) {
            PxLog::log.info("Operation was not confirmed.");
            exit(1);
        }

        std::vector<std::string> toFetch = { "pxos-" + version + ".img" };

        PXASSERT(clear_fetch_files(toFetch));
        for (auto &fetch : toFetch) {
            if (system(("curl -#LfC - -o /var/tmp/"+fetch+" "+osconf.repo+"/"+fetch).c_str()) != 0) {
                PxLog::log.error("Download failed! Exiting...");
                exit(1);
            }
        }

        PXASSERT(replace("/var/tmp/pxos-"+version+".img"));
        PXASSERT(clear_fetch_files({}));
        PxLog::log.info("Finished update.");
    } else {
        PxLog::log.info("No updates available.");
    }
    return PxResult::Null;
}
PxResult::Result<void> cmd_replace(std::vector<std::string> &extra_args) {
    return replace(extra_args[0]);
}

std::vector<command_t> commands = {
    {
        .name = "update",
        .help = "Update the system",
        .needsRoot = true,
        .action = cmd_update
    },
    {
        .name = "replace",
        .help = "Replace the current image",
        .needsRoot = true,
        .action = cmd_replace
    }
};

int main(int argc, const char* argv[]) {
    PxArg::SelectArgument command("COMMAND", "Command to run", false);
    PxArg::Argument help("help", 'h', "Print help");

    for (auto &i : commands) {
        command.addOption(i.name, i.help);
    }

    PxArg::ArgParser parser({&command}, {&help});

    auto extra_argsres = parser.parseArgs(PxFunction::vectorize(argc, argv));

    if (help.active) {
        parser.printHelp();
        return 0;
    }

    if (extra_argsres.eno) {
        PxLog::log.error("Bad argument.");
        PxLog::log.info((std::string)"Try \""+argv[0]+" --help\" for help");
        return 1;
    }

    auto extra_args = extra_argsres.assert();

    // load config

    auto confres = PxConfig::ReadConfig("/etc/pxos.conf");
    if (confres.eno) {
        PxLog::log.error("Failed to load config: "+confres.funcName+": "+strerror(confres.eno));
        exit(1);
    }
    PxConfig::conf baseconf = confres.assert();

    osconf = {
        .repo = baseconf.QuickRead("repo"),
        .branch = baseconf.QuickRead("branch")
    };

    for (auto &i : commands) {
        if (i.name == command.value) {
            if (i.needsRoot && geteuid() != 0) {
                PxLog::log.error("Must be root!");
                exit(1);
            }
            auto res = i.action(extra_args);
            if (res.eno) {
                PxLog::log.error("Error: " + res.funcName + ": " + strerror(errno));
            }
            goto end;
        }
    }

no_command:
    PxLog::log.error("No such command: " + command.value);
    return 1;

end:
    return 0;
}