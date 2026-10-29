// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/EntryNames.h"

#include "ThirdParty/doctest.h"

using namespace Dup::Core;

namespace
{
const std::string D(1, static_cast<char>(std::filesystem::path::preferred_separator));
}

TEST_CASE("SafeRelativePath refuses traversal, absolute paths and drive letters")
{
    CHECK(SafeRelativePath("../../etc/passwd", "/", 0) == "etc" + D + "passwd");
    CHECK(SafeRelativePath("/etc/passwd", "/", 0) == "etc" + D + "passwd");
    CHECK(SafeRelativePath("C:\\Windows\\x.dll", "\\", 0) == "Windows" + D + "x.dll");
    CHECK(SafeRelativePath("a\\..\\..\\b", "\\", 0) == "a" + D + "b");
    CHECK(SafeRelativePath("a//b/./c", "/", 0) == "a" + D + "b" + D + "c");
    CHECK(SafeRelativePath("..", "/", 7) == "entry_7");
    CHECK(SafeRelativePath("", "", 3) == "entry_3");
}

TEST_CASE("SafeRelativePath on flat archives keeps the separators in the name")
{
    CHECK(SafeRelativePath("..\\..\\x", "", 0).find(D) == std::string::npos);
    CHECK(SafeRelativePath("a/b", "", 0).find(D) == std::string::npos);
#ifndef _WIN32
    CHECK(SafeRelativePath("VILE\\A1", "", 0) == "VILE\\A1");
#endif
    CHECK(SafeRelativePath(std::string("a\0b\x07" "c", 5), "", 0) == "a_b_c");
    CHECK(SafeRelativePath("MAP[01]", "", 0) == "MAP[01]");
}

TEST_CASE("Long names are shortened with their extension, without cutting a UTF-8 character")
{
    const auto path = SafeRelativePath(std::string(500, 'x') + ".txt", "", 0);
    CHECK(path.size() <= 200);
    CHECK(path.substr(path.size() - 4) == ".txt");
    std::string accented;
    for (int i = 0; i < 150; ++i)
    {
        accented += "\xc3\xa9"; // e acute, two bytes
    }
    const auto shortened = SafeComponent(accented);
    CHECK(shortened.size() <= 200);
    CHECK(shortened.size() % 2 == 0);
}

TEST_CASE("IsInsideDir")
{
    CHECK(IsInsideDir("/tmp/out", "/tmp/out/a/b"));
    CHECK_FALSE(IsInsideDir("/tmp/out", "/tmp/outside/a"));
    CHECK_FALSE(IsInsideDir("/tmp/out", "/tmp/out/../x"));
    CHECK_FALSE(IsInsideDir("/tmp/out", "/tmp/out"));
}

TEST_CASE("NameAllocator makes names unique")
{
    NameAllocator allocator;
    CHECK(allocator.Allocate("THINGS") == "THINGS");
    CHECK(allocator.Allocate("THINGS") == "THINGS~2");
    CHECK(allocator.Allocate("things") == "things~3");
    CHECK(allocator.Allocate("a.txt") == "a.txt");
    CHECK(allocator.Allocate("a.txt") == "a~2.txt");
    CHECK(allocator.Allocate("d") == "d");
    CHECK(allocator.Allocate("d" + D + "x") == "d~2" + D + "x");
    CHECK(allocator.Allocate("e" + D + "x") == "e" + D + "x");
    CHECK(allocator.Allocate("e") == "e~2");
}

TEST_CASE("NaturalCompare")
{
    CHECK(NaturalCompare("file2", "file10") < 0);
    CHECK(NaturalCompare("file10", "file2") > 0);
    CHECK(NaturalCompare("File1", "file1") == 0);
    CHECK(NaturalCompare("a007", "a7") == 0);
    CHECK(NaturalCompare("MAP01", "MAP02") < 0);
    CHECK(NaturalCompare("abc", "abcd") < 0);
    CHECK(NaturalCompare("", "a") < 0);
    CHECK(NaturalCompare("x1y", "x1z") < 0);
}
