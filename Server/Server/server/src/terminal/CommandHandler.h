#pragma once

#include <query/Command.h>
#include <Error.h>

namespace terminal::chandler {
    struct TerminalCommand {
        std::string line;

        std::string command;
        std::string lcommand;

        std::vector<variable> arguments;
        std::vector<std::string> larguments;
    };

    struct CommandHandle {
        std::string command{};
        std::deque<std::string> response{};
    };

    extern bool handleCommand(CommandHandle& /* command */);

    extern bool handleCommandDummyCrash(CommandHandle& /* handle */, TerminalCommand&);
    extern bool handleCommandDummyFdFlood(CommandHandle& /* handle */, TerminalCommand&);

    extern bool handleCommandHelp(CommandHandle& /* handle */, TerminalCommand&);
    extern bool handleCommandEnd(CommandHandle& /* handle */, TerminalCommand&);
    extern bool handleCommandInfo(CommandHandle& /* handle */, TerminalCommand&);
    extern bool handleCommandChat(CommandHandle& /* handle */, TerminalCommand&);

    extern bool handleCommandPermGrant(CommandHandle& /* handle */, TerminalCommand&);

    extern bool handleCommandMemFlush(CommandHandle& /* handle */, TerminalCommand&);
    extern bool handleCommandMemInfo(CommandHandle& /* handle */, TerminalCommand&);
    extern bool handleCommandSpoken(CommandHandle& /* handle */, TerminalCommand&);

    extern bool handleCommandPasswd(CommandHandle& /* handle */, TerminalCommand&);

    extern bool handleCommandStatsReset(CommandHandle& /* handle */, TerminalCommand&);

    extern bool handleCommandReload(CommandHandle& /* handle */, TerminalCommand&);
    extern bool handleCommandTaskInfo(CommandHandle& /* handle */, TerminalCommand&);
}