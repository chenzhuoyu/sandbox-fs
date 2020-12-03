#include <csignal>
#include <iostream>
#include <stdexcept>
#include <gflags/gflags.h>

#include <folly/init/Init.h>
#include <folly/logging/Init.h>
#include <folly/logging/xlog.h>
#include <folly/logging/LogFormatter.h>

#include "file_node.h"
#include "fuse_error.h"
#include "control_interface.h"
#include "sandbox_controller.h"
#include "sandbox_file_system.h"

FOLLY_INIT_LOGGING_CONFIG(
    ".=INFO;"
    "default:"
    "async"           "=" "true,"
    "sync_level"      "=" "WARNING,"
    "max_buffer_size" "=" "1048576,"
    "stream"          "=" "stdout,"
    "formatter"       "=" "custom,"
    "colored"         "=" "auto,"
    "log_format"      "=" "{H:02}:{M:02}:{S:02}.{USECS:06} [{L}]"
);

#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err58-cpp"

DEFINE_string(o, "", "VFS mount options");

#pragma clang diagnostic pop

int main(int argc, char **argv) {
    google::SetUsageMessage("[OPTIONS] mountpoint");
    google::SetVersionString("v1.0");

    /* initialize folly & control interface */
    folly::Init        init  (&argc, &argv);
    FileNode::Node     root  (SandboxController::root());
    ControlInterface * iface (SandboxController::iface());

    /* mount point cannot be empty */
    if (argc < 2) {
        std::cerr << "* error: mountpoint is not specified." << std::endl;
        google::ShowUsageWithFlags(argv[0]);
        return 1;
    }

    /* can have at most 1 mount point */
    if (argc > 2) {
        std::cerr << "* error: multiple mountpoints is not supported." << std::endl;
        google::ShowUsageWithFlags(argv[0]);
        return 1;
    }

    /* reset signal handlers */
    signal(SIGINT, SIG_DFL);
    signal(SIGHUP, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);

    /* start the file system */
    try {
        SandboxController::Guard _;
        SandboxFileSystem(root, iface).start(argv[1], FLAGS_o);
    } catch (const FuseError &e) {
        XLOGF(ERR, "* error: FuseError: [{:d}] {:s}.", e.code(), e.message());
        return e.code();
    }

    /* all done */
    XLOG(INFO, "Bye.");
    return 0;
}
