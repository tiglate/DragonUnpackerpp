// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/ExternalTools.h"
#include "TestSupport.h"

#include <fstream>

using namespace Dup::Core;

namespace
{
void Write(const std::filesystem::path &path, const std::string &text)
{
    std::ofstream(path) << text;
}
} // namespace

TEST_CASE("Command lines are split at the spaces outside of quotes")
{
    CHECK(SplitCommand("-o \"%o\" \"%i\"") == std::vector<std::string>{"-o", "%o", "%i"});
    CHECK(SplitCommand("  a   \"b c\"  d ") == std::vector<std::string>{"a", "b c", "d"});
    CHECK(SplitCommand("\"\" x") == std::vector<std::string>{"", "x"});
    CHECK(SplitCommand("").empty());
}

TEST_CASE("The exit code tests of v5")
{
    ExternalTool tool;
    tool.ResultOk = 2;
    tool.ResultTest = 0;
    CHECK(IsResultOk(tool, 2));
    CHECK(!IsResultOk(tool, 3));
    tool.ResultTest = 3;
    CHECK(IsResultOk(tool, 2));
    CHECK(IsResultOk(tool, 5));
    tool.ResultTest = 4;
    CHECK(IsResultOk(tool, 1));
    CHECK(!IsResultOk(tool, 3));
    tool.ResultTest = 1;
    CHECK(IsResultOk(tool, 3));
    CHECK(!IsResultOk(tool, 2));
    tool.ResultTest = 2;
    CHECK(IsResultOk(tool, 1));
    tool.ResultTest = 5;
    CHECK(IsResultOk(tool, 0));
    CHECK(!IsResultOk(tool, 2));
}

TEST_CASE("The INI files of the tools are read and checked")
{
    Dup::Tests::TempDirectory directory;
    Write(directory.GetPath() / "copy.ini",
          "[cnv_exttools]\nenabled=1\nname=Copy\nauthor=me\nurl=http://x\ncomment=c\npath=cp\ncommand=\"%i\" \"%o\"\nresultext=DAT\nresultoktest=0\nresultok=0\nextensions=bin TXT\n");
    Write(directory.GetPath() / "off.ini", "[cnv_exttools]\nenabled=0\nname=Off\npath=cp\ncommand=\"%i\" \"%o\"\nresultext=dat\nextensions=bin\n");
    Write(directory.GetPath() / "missing.ini", "[cnv_exttools]\nenabled=1\nname=Missing\npath=no-such-program-here\ncommand=\"%i\" \"%o\"\nresultext=dat\nextensions=bin\n");
    Write(directory.GetPath() / "nooutput.ini", "[cnv_exttools]\nenabled=1\nname=No output\npath=cp\ncommand=\"%i\"\nresultext=dat\nextensions=bin\n");
    Write(directory.GetPath() / "other.ini", "[something]\nname=x\n");
    Write(directory.GetPath() / "readme.txt", "not an ini file");

    const auto tools = LoadExternalTools(directory.GetPath());
    REQUIRE(tools.size() == 4); // other.ini has no [cnv_exttools] section
    ExternalTool copy;
    for (const auto &tool : tools)
    {
        if (tool.Name == "Copy")
        {
            copy = tool;
        }
        else
        {
            CHECK(!tool.IsUsable());
        }
    }
    CHECK(copy.Author == "me");
    CHECK(copy.ResultExtension == "dat");
    CHECK(copy.Handles("bin"));
    CHECK(copy.Handles("TXT"));
    CHECK(!copy.Handles("png"));
#ifndef _WIN32
    REQUIRE(copy.IsUsable());
    const std::vector<std::byte> input{std::byte{1}, std::byte{2}, std::byte{3}};
    const auto output = RunExternalTool(copy, input, "bin");
    REQUIRE(output);
    CHECK(*output == input);
    // a program that writes nothing gives nothing
    ExternalTool silent = copy;
    silent.Command = "\"%i\" \"%o\"";
    silent.Program = "/bin/true";
    CHECK(!RunExternalTool(silent, input, "bin"));
    // names in the extension cannot reach the command line
    CHECK(RunExternalTool(copy, input, "b;in && x"));
#endif
    CHECK(LoadExternalTools(directory.GetPath() / "nothing here").empty());
}
