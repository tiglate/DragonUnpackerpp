// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/ArchiveIndex.h"

#include "ThirdParty/doctest.h"

using Dup::Core::ArchiveIndex;

TEST_CASE("ArchiveIndex builds the folder tree")
{
    const auto index = ArchiveIndex::FromNames({"a/b/c.txt", "a/b/d.txt", "a/e.txt", "root.txt", "a/b/c.txt", "x//y"}, "/");
    CHECK(index.GetEntryCount() == 6);
    CHECK(index.GetFolders().size() == 4); // root, a, a/b, x
    CHECK(index.GetFolders()[ArchiveIndex::ROOT].FileCount == 6);
    CHECK(index.GetFolders()[ArchiveIndex::ROOT].Entries.size() == 1);
    CHECK(index.GetFileName(0) == "c.txt");
    CHECK(index.GetFolderOf(0) == index.GetFolderOf(4));
    CHECK(index.GetEntriesRecursive(ArchiveIndex::ROOT).size() == 6);
}

TEST_CASE("A backslash archive splits on / too")
{
    const auto index = ArchiveIndex::FromNames({"a\\b", "a/c", "A\\d"}, "\\");
    CHECK(index.GetFolders().size() == 3); // root, a, A: the case matters
}

TEST_CASE("The same folder name at two levels is not merged")
{
    const auto index = ArchiveIndex::FromNames({"a/a/f", "a/f", "f"}, "/");
    CHECK(index.GetFolders().size() == 3);
}

TEST_CASE("A flat archive has no folders")
{
    const auto index = ArchiveIndex::FromNames({"VILE\\A1", "a/b"}, "");
    CHECK(index.GetFolders().size() == 1);
    CHECK(index.GetFileName(1) == "a/b");
}
