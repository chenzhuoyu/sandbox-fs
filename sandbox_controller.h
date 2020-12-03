#ifndef SANDBOX_FS_SANDBOX_CONTROLLER_H
#define SANDBOX_FS_SANDBOX_CONTROLLER_H

#include <iostream>
#include <unordered_map>
#include <nlohmann/json.hpp>

#include "file_node.h"
#include "control_interface.h"

static constexpr mode_t     Mode   = S_IFREG | 0644;
static constexpr const char Name[] = "_fsctl";

class SandboxController : public ControlInterfaceAdapter<SandboxController, Name, Mode> {
    std::stringbuf _rbuf;
    std::stringbuf _wbuf;

public:
    using JSON        = nlohmann::json;
    using CommandArgs = std::unordered_map<std::string, JSON>;
    using ControlInterfaceAdapter::ControlInterfaceAdapter;

public:
    ssize_t do_read(char *buf, size_t len, size_t off) override;
    ssize_t do_write(const char *buf, size_t len, size_t off) override;

private:
    void reply(const JSON &v);
    void fireCommand(const char *buf, size_t len);
    void executeCommand(const std::string &cmd, const CommandArgs &args);

#define DECLARE_CMD_1(name, type0, arg0)                                \
    void execute_ ## name(type0 arg0);                                  \
    void execute_ ## name(const CommandArgs &args) {                    \
        execute_ ## name(args.at(#arg0).get<std::decay_t<type0>>());    \
    }

#define DECLARE_CMD_2(name, type0, arg0, type1, arg1)                   \
    void execute_ ## name(type0 arg0, type1 arg1);                      \
    void execute_ ## name(const CommandArgs &args) {                    \
        execute_ ## name(                                               \
            args.at(#arg0).get<std::decay_t<type0>>(),                  \
            args.at(#arg1).get<std::decay_t<type1>>()                   \
        );                                                              \
    }

private:
    DECLARE_CMD_1(LOAD, const std::string &, file)
    DECLARE_CMD_2(MOUNT, const std::string &, token, const std::string &, alias)
    DECLARE_CMD_1(UNLOAD, const std::string &, token)
    DECLARE_CMD_1(UNMOUNT, const std::string &, alias)

#undef DECLARE_CMD_1
#undef DECLARE_CMD_2

public:
    struct Guard {
        ~Guard() { end(); }
    };

public:
    static void             end();
    static FileNode::Node & root();
};

#endif /* SANDBOX_FS_SANDBOX_CONTROLLER_H */
