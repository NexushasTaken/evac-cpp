#include "core/process.h"

#include <cctype>

namespace process
{

static bool starts_with(const std::string& text, const std::string& prefix, bool ignore_case)
{
    if (text.size() < prefix.size())
        return false;
    for (size_t i = 0; i < prefix.size(); ++i)
    {
        char a = text[i];
        char b = prefix[i];
        if (ignore_case)
        {
            a = (char)std::toupper((unsigned char)a);
            b = (char)std::toupper((unsigned char)b);
        }
        if (a != b)
            return false;
    }
    return true;
}

std::vector<std::string> apply_environment(const std::vector<std::string>& current, const Environment& env,
                                           bool ignore_case)
{
    std::vector<std::string> result;
    for (size_t i = 0; i < current.size(); ++i)
    {
        bool drop = false;
        for (size_t p = 0; p < env.remove_prefixes.size() && !drop; ++p)
            drop = starts_with(current[i], env.remove_prefixes[p], ignore_case);
        for (size_t s = 0; s < env.set.size() && !drop; ++s)
            drop = starts_with(current[i], env.set[s].first + "=", ignore_case);
        if (!drop)
            result.push_back(current[i]);
    }
    for (size_t s = 0; s < env.set.size(); ++s)
        result.push_back(env.set[s].first + "=" + env.set[s].second);
    return result;
}

// The rules are those of CommandLineToArgvW: backslashes are only special
// right before a double quote (or the closing quote).
std::string windows_quote_arg(const std::string& arg)
{
    if (!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string::npos)
        return arg;

    std::string result = "\"";
    size_t i = 0;
    while (true)
    {
        size_t backslashes = 0;
        while (i < arg.size() && arg[i] == '\\')
        {
            ++backslashes;
            ++i;
        }
        if (i == arg.size())
        {
            // Double them, so the closing quote stays a quote.
            result.append(backslashes * 2, '\\');
            break;
        }
        if (arg[i] == '"')
        {
            result.append(backslashes * 2 + 1, '\\');
            result += '"';
        }
        else
        {
            result.append(backslashes, '\\');
            result += arg[i];
        }
        ++i;
    }
    return result + "\"";
}

std::string windows_command_line(const std::string& program, const std::vector<std::string>& args)
{
    // The program name is read differently: up to the next quote, no escapes.
    // Windows file names can't contain a quote, so plain quotes are enough.
    std::string line = program.find_first_of(" \t") == std::string::npos ? program : "\"" + program + "\"";
    for (size_t i = 0; i < args.size(); ++i)
        line += " " + windows_quote_arg(args[i]);
    return line;
}

}  // namespace process
