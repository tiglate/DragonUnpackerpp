// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/ExternalTools.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <fcntl.h>
#include <cctype>
#include <fstream>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace Dup::Core
{
namespace
{
auto Trim(const std::string &text) -> std::string
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return {};
    }
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

auto Lower(std::string text) -> std::string
{
    for (auto &c : text)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

auto ParseBool(const std::string &value) -> bool
{
    const std::string v = Lower(value);
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

auto ParseInt(const std::string &value, int fallback) -> int
{
    try
    {
        return std::stoi(value);
    }
    catch (const std::exception &)
    {
        return fallback;
    }
}

// Where the program is: as written, relative to the folder of the INI files, or on the PATH
auto LocateProgram(const std::string &path, const std::filesystem::path &directory) -> std::filesystem::path
{
    if (path.empty())
    {
        return {};
    }
    std::error_code ignored;
    std::filesystem::path program = path;
    // the tools of v5 were written with "\" as separator
    std::string portable = path;
    std::replace(portable.begin(), portable.end(), '\\', '/');
    for (const auto &candidate : {std::filesystem::path(path), std::filesystem::path(portable), directory / path, directory / portable})
    {
        if (std::filesystem::is_regular_file(candidate, ignored))
        {
            return candidate;
        }
    }
    if (path.find_first_of("/\\") == std::string::npos)
    {
        const char *search = std::getenv("PATH");
        if (search != nullptr)
        {
            std::stringstream list(search);
            std::string part;
#ifdef _WIN32
            constexpr char SEPARATOR = ';';
#else
            constexpr char SEPARATOR = ':';
#endif
            while (std::getline(list, part, SEPARATOR))
            {
                for (const std::string suffix : {"", ".exe"})
                {
                    const std::filesystem::path candidate = std::filesystem::path(part) / (path + suffix);
                    if (std::filesystem::is_regular_file(candidate, ignored))
                    {
                        return candidate;
                    }
                }
            }
        }
    }
    return {};
}

std::atomic<unsigned> temporaryCounter{0};
} // namespace

auto ExternalTool::IsUsable() const -> bool
{
    return Enabled && !Program.empty() && Command.find("%i") != std::string::npos && Command.find("%o") != std::string::npos;
}

auto ExternalTool::Handles(const std::string &extension) const -> bool
{
    return std::find(Extensions.begin(), Extensions.end(), Lower(extension)) != Extensions.end();
}

auto LoadExternalTools(const std::filesystem::path &directory) -> std::vector<ExternalTool>
{
    std::vector<ExternalTool> tools;
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error))
    {
        return tools;
    }
    std::vector<std::filesystem::path> files;
    for (const auto &item : std::filesystem::directory_iterator(directory, error))
    {
        if (item.is_regular_file() && Lower(item.path().extension().string()) == ".ini")
        {
            files.push_back(item.path());
        }
    }
    std::sort(files.begin(), files.end());
    for (const auto &file : files)
    {
        std::ifstream stream(file);
        std::string line;
        bool inSection = false;
        bool found = false;
        ExternalTool tool;
        tool.IniName = file.filename().string();
        while (std::getline(stream, line))
        {
            line = Trim(line);
            if (line.empty() || line.front() == ';' || line.front() == '#')
            {
                continue;
            }
            if (line.front() == '[')
            {
                inSection = Lower(line) == "[cnv_exttools]";
                found = found || inSection;
                continue;
            }
            const auto equals = line.find('=');
            if (!inSection || equals == std::string::npos)
            {
                continue;
            }
            const std::string key = Lower(Trim(line.substr(0, equals)));
            const std::string value = Trim(line.substr(equals + 1));
            if (key == "enabled")
            {
                tool.Enabled = ParseBool(value);
            }
            else if (key == "name")
            {
                tool.Name = value;
            }
            else if (key == "author")
            {
                tool.Author = value;
            }
            else if (key == "url")
            {
                tool.Url = value;
            }
            else if (key == "comment")
            {
                tool.Comment = value;
            }
            else if (key == "path")
            {
                tool.Path = value;
            }
            else if (key == "command")
            {
                tool.Command = value;
            }
            else if (key == "resultext")
            {
                tool.ResultExtension = Lower(value);
            }
            else if (key == "resultoktest")
            {
                tool.ResultTest = ParseInt(value, 0);
            }
            else if (key == "resultok")
            {
                tool.ResultOk = ParseInt(value, 0);
            }
            else if (key == "extensions")
            {
                std::stringstream list(Lower(value));
                std::string extension;
                while (list >> extension)
                {
                    tool.Extensions.push_back(extension);
                }
            }
        }
        if (found)
        {
            tool.Program = LocateProgram(tool.Path, directory);
            if (tool.Name.empty())
            {
                tool.Name = tool.IniName;
            }
            tools.push_back(std::move(tool));
        }
    }
    return tools;
}

auto DefaultToolsDirectory(const std::filesystem::path &settingsFile) -> std::filesystem::path
{
    return settingsFile.parent_path() / "tools";
}

auto SplitCommand(const std::string &command) -> std::vector<std::string>
{
    std::vector<std::string> arguments;
    std::string current;
    bool quoted = false;
    bool any = false;
    for (const char c : command)
    {
        if (c == '"')
        {
            quoted = !quoted;
            any = true;
        }
        else if (!quoted && (c == ' ' || c == '\t'))
        {
            if (any)
            {
                arguments.push_back(current);
                current.clear();
                any = false;
            }
        }
        else
        {
            current += c;
            any = true;
        }
    }
    if (any)
    {
        arguments.push_back(current);
    }
    return arguments;
}

auto IsResultOk(const ExternalTool &tool, int exitCode) -> bool
{
    switch (tool.ResultTest)
    {
    case 0:
        return exitCode == tool.ResultOk;
    case 1:
        return exitCode > tool.ResultOk;
    case 2:
        return exitCode < tool.ResultOk;
    case 3:
        return exitCode >= tool.ResultOk;
    case 4:
        return exitCode <= tool.ResultOk;
    case 5:
        return exitCode != tool.ResultOk;
    default:
        return false;
    }
}

namespace
{
// Runs a program with its arguments (no shell); the exit code, or -1 when it could not be started
auto RunProgram(const std::filesystem::path &program, const std::vector<std::string> &arguments) -> int
{
#ifdef _WIN32
    std::wstring line = L"\"" + program.wstring() + L"\"";
    for (const auto &argument : arguments)
    {
        const std::wstring wide = std::filesystem::path(argument).wstring();
        line += L" \"" + wide + L"\"";
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(program.wstring().c_str(), line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
    {
        return -1;
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    return static_cast<int>(code);
#else
    std::vector<std::string> storage{program.string()};
    storage.insert(storage.end(), arguments.begin(), arguments.end());
    std::vector<char *> argv;
    for (auto &item : storage)
    {
        argv.push_back(item.data());
    }
    argv.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, 1, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);
    pid_t pid = 0;
    const int started = posix_spawn(&pid, storage[0].c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (started != 0)
    {
        return -1;
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
    {
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}
} // namespace

auto RunExternalTool(const ExternalTool &tool, std::span<const std::byte> input, const std::string &inputExtension) -> std::optional<std::vector<std::byte>>
{
    if (!tool.IsUsable())
    {
        return std::nullopt;
    }
    namespace fs = std::filesystem;
    std::error_code error;
    // the names are made here: nothing of the name of the entry reaches the command line but its extension
    std::string safeExtension;
    for (const char c : inputExtension)
    {
        if (std::isalnum(static_cast<unsigned char>(c)))
        {
            safeExtension += c;
        }
    }
    const fs::path base = fs::temp_directory_path(error);
    if (error)
    {
        return std::nullopt;
    }
    const std::string stem = "dup_tool_" + std::to_string(static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(&tool)) & 0xFFFFFF) + "_" + std::to_string(++temporaryCounter);
    const fs::path inputFile = base / (stem + "_in" + (safeExtension.empty() ? "" : "." + safeExtension));
    const fs::path outputFile = base / (stem + "_out." + (tool.ResultExtension.empty() ? "out" : tool.ResultExtension));
    struct Cleanup
    {
        fs::path A, B;
        ~Cleanup()
        {
            std::error_code ignored;
            fs::remove(A, ignored);
            fs::remove(B, ignored);
        }
    } cleanup{inputFile, outputFile};
    {
        std::ofstream file(inputFile, std::ios::binary);
        if (!file)
        {
            return std::nullopt;
        }
        file.write(reinterpret_cast<const char *>(input.data()), static_cast<std::streamsize>(input.size()));
        if (!file)
        {
            return std::nullopt;
        }
    }
    std::vector<std::string> arguments = SplitCommand(tool.Command);
    for (auto &argument : arguments)
    {
        for (const auto &[token, value] : {std::pair<std::string, std::string>{"%i", inputFile.string()}, {"%o", outputFile.string()}})
        {
            for (auto at = argument.find(token); at != std::string::npos; at = argument.find(token, at + value.size()))
            {
                argument.replace(at, token.size(), value);
            }
        }
    }
    const int code = RunProgram(tool.Program, arguments);
    std::ifstream result(outputFile, std::ios::binary);
    if (!result)
    {
        return std::nullopt;
    }
    std::vector<char> raw((std::istreambuf_iterator<char>(result)), std::istreambuf_iterator<char>());
    if (raw.empty() || (code < 0))
    {
        return std::nullopt;
    }
    // as in v5, the exit code does not matter when the tool wrote something
    std::vector<std::byte> bytes(raw.size());
    std::memcpy(bytes.data(), raw.data(), raw.size());
    return bytes;
}
} // namespace Dup::Core
