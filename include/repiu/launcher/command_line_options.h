#ifndef REPIU_LAUNCHER_COMMAND_LINE_OPTIONS_H_
#define REPIU_LAUNCHER_COMMAND_LINE_OPTIONS_H_

#include <string>
#include <vector>

namespace repiu::launcher
{

// Task 771. Options taken off the command line before the ROM set argument is
// read. Each one is delivered by publishing the environment variable its
// consumer already reads, so the rest of the loader sees the same positional
// arguments it always has.
struct CommandLineOptions
{
    // Everything that is not an option, in order: the ROM set or executable
    // path the loader reads as its first argument, and anything after it.
    std::vector<std::string> positional;
    // `--post-shader <id>` or `--post-shader=<id>`; the last one given wins.
    // The id is checked by the engine, not here.
    bool has_post_shader = false;
    std::string post_shader;
    // Non-empty when the command line is malformed; the loader stops on it.
    std::string error;
};

// Parses the arguments after the program name. Arguments it does not know are
// passed through as positional, so nothing that ran before changes meaning;
// `--` ends option parsing.
[[nodiscard]] CommandLineOptions ParseCommandLineOptions(
    const std::vector<std::string>& arguments);

}  // namespace repiu::launcher

#endif  // REPIU_LAUNCHER_COMMAND_LINE_OPTIONS_H_
