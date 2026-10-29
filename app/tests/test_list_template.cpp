// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/ListTemplate.h"
#include "TestSupport.h"

#include <cstring>
#include <map>

using namespace Dup::Core;

namespace
{
auto Bytes(const std::string &text) -> std::vector<std::byte>
{
    std::vector<std::byte> b(text.size());
    std::memcpy(b.data(), text.data(), text.size());
    return b;
}

const std::map<std::string, std::string> FILES{{"header.txt", "$$FILENAME$$ [$$FORMAT$$] ($$FTOTNUMBYTES$$ bytes) ($$TOTNUMFILES$$ files) dir=$$DIRECTORY$$ $$LNG<LSTCP1>$$\n"},
                                              {"variable.txt", "$$DIRECTORY$$|$$file$$|$$FILE0$$|$$FILEEXT$$|$$SIZE$$|$$FSIZE$$|$$HSIZE$$|$$OFFSET$$|$$HOFFSET$$|$$DATAX$$|$$DESC$$|$$unknown$$\n"},
                                              {"footer.txt", "END $$TOTNUMBYTES$$\n"},
                                              {"preview.jpg", "JPEGDATA"},
                                              {"logo.gif", "GIFDATA"}};

auto Reader() -> TemplateFileReader
{
    return [](const std::string &name) -> std::optional<std::vector<std::byte>> {
        const auto found = FILES.find(name);
        if (found == FILES.end())
        {
            return std::nullopt;
        }
        return Bytes(found->second);
    };
}

const char *const SOURCE = R"(# a template
{TSF}
Name = Test list
Version = 1.2
Author = Me
Email = me@example.org
URL = http://example.org
Compression = 1
OutFile = test.uht
DefaultExt = TXT
ExtInfo = Test (*.TXT)
HeaderFile = header.txt
FooterFile = footer.txt
VariableFile = variable.txt
Preview = preview.jpg
IncludeFile = logo.gif
{/TSF}
)";

auto Entries() -> std::vector<ListEntry>
{
    return {{"dir\\sub\\b.WAV", 4096, 2048, 7, 0}, {"a.txt", 10, 1234567, 0, 0}, {"dir\\c", 100000, 5, 0, 0}};
}
} // namespace

TEST_CASE("The source of a template is read as DUHTComp did")
{
    const auto list = ParseTsf(SOURCE, Reader());
    CHECK(list.Info.Name == "Test list");
    CHECK(list.Info.Author == "Me");
    CHECK(list.Info.Email == "me@example.org");
    CHECK(list.Info.DefaultExtension == "TXT");
    CHECK(list.Compress);
    REQUIRE(list.Images.size() == 1);
    CHECK(list.Images[0].Name == "logo.gif");
    CHECK(list.Preview.size() == 8);
    CHECK_THROWS_WITH(ParseTsf("{TSF}\nBogus = 1\n{/TSF}\n", Reader()), "line 2: unknown header keyword [BOGUS]");
    CHECK_THROWS(ParseTsf("Name = x\n", Reader()));                       // outside of {TSF}
    CHECK_THROWS(ParseTsf("{TSF}\nHeaderFile = nothing.txt\n{/TSF}\n", Reader())); // a file that does not exist
    CHECK_THROWS(ParseTsf("{TSF}\nName only\n{/TSF}\n", Reader()));
    CHECK_THROWS(ParseTsf("{TSF}\nName = x\n{/TSF}\n", Reader())); // no variable part
}

TEST_CASE("The list is made with the macros of v5")
{
    const auto list = ParseTsf(SOURCE, Reader());
    ListContext context;
    context.ArchiveName = "game.pak";
    context.Format = "PACK";
    context.Separator = "\\";
    context.Language = [](const std::string &keyword) { return keyword == "LSTCP1" ? std::string("File") : std::string(); };
    context.Describe = [](const std::string &extension) { return DescribeExtension(extension); };
    const std::string text = RenderList(list, context, Entries());
    CHECK(text.rfind("game.pak [PACK] (1,236,620 bytes) (3 files) dir= File\n", 0) == 0);
    CHECK(text.find("dir\\sub\\|b.WAV|b|WAV|2048|2,048|00000800|4096|0000000000001000|7|Sound (RIFF/WAVE)|$$unknown$$\n") != std::string::npos);
    CHECK(text.find("|a.txt|a|txt|1234567|1,234,567|0012D687|10|000000000000000A|0|Text Document|$$unknown$$\n") != std::string::npos);
    CHECK(text.find("dir\\|c|c||5|5|00000005|100000|00000000000186A0|0|File |") != std::string::npos);
    CHECK(text.substr(text.size() - 12) == "END 1236620\n");
}

TEST_CASE("A compiled template is read back, compressed or not")
{
    const auto list = ParseTsf(SOURCE, Reader());
    for (const bool compress : {true, false})
    {
        INFO("compress " << compress);
        const auto bytes = CompileUht(list, compress);
        const auto back = ParseUht(bytes);
        CHECK(back.Info.Name == list.Info.Name);
        CHECK(back.Info.Version == "1.2");
        CHECK(back.Info.Url == "http://example.org");
        CHECK(back.Info.ExtensionInfo == "Test (*.TXT)");
        CHECK(back.Header == list.Header);
        CHECK(back.Footer == list.Footer);
        CHECK(back.Variable == list.Variable);
        CHECK(back.Preview == list.Preview);
        REQUIRE(back.Images.size() == 1);
        CHECK(back.Images[0].Name == "logo.gif");
        CHECK(back.Images[0].Data == list.Images[0].Data);
        CHECK(RenderList(back, {}, Entries()) == RenderList(list, {}, Entries()));
    }
    // the bytes of the header are those of the format
    const auto bytes = CompileUht(list, false);
    CHECK(std::memcmp(bytes.data(), "DUHT\x1A\x02\x01", 7) == 0);
    auto bad = bytes;
    bad.resize(40);
    CHECK_THROWS(ParseUht(bad));
    CHECK_THROWS(ParseUht(Bytes("not a template at all, not at all")));
    // a part that does not unpack
    const auto packed = CompileUht(list, true);
    auto damaged = packed;
    for (std::size_t i = damaged.size() - 60; i < damaged.size() - 30; ++i)
    {
        damaged[i] = std::byte{0x55};
    }
    CHECK_THROWS(ParseUht(damaged));
}

TEST_CASE("Entries are sorted by name, size or offset")
{
    auto entries = Entries();
    SortEntries(entries, ListSort::NAME, false);
    CHECK(entries[0].Name == "a.txt");
    CHECK(entries[2].Name == "dir\\sub\\b.WAV");
    SortEntries(entries, ListSort::SIZE, true);
    CHECK(entries[0].Size == 1234567);
    CHECK(entries[2].Size == 5);
    SortEntries(entries, ListSort::OFFSET, false);
    CHECK(entries[0].Offset == 10);
    CHECK(entries[2].Offset == 100000);
}

TEST_CASE("Descriptions of the types of files")
{
    CHECK(DescribeExtension("bmp") == "Image BMP (Windows BitMaP)");
    CHECK(DescribeExtension("DLL") == "Dynamic Link Library");
    CHECK(DescribeExtension("it") == "Music (Impulse Tracker)");
    CHECK(DescribeExtension("xyz") == "File XYZ");
    CHECK(DescribeExtension("") == "File ");
    const auto french = [](const std::string &keyword) { return keyword == "EXTXT" ? std::string("Document texte") : std::string(); };
    CHECK(DescribeExtension("txt", french) == "Document texte");
    CHECK(DescribeExtension("wav", french) == "Sound (RIFF/WAVE)"); // what is not given stays English
}
