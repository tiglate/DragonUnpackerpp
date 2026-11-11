// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Requiem: Avenging Angel animation pack (Models/AnimFile.ary + AnimFile.bin). Either file can be opened, the other
// is looked for in the same directory. The .ARY index is a u32 count, then per record 16 bytes: offset u32, 0, size
// u32, 0xCDCDCDCD (left over memory of the tool that wrote it); the records follow one another and fill the .BIN.
// Each record is an animation file (the layout is in codecs/RequiemModel.cpp), listed as it is (anim_NNN.bin).
// When AnimFile.tab (model id * 40 + animation number -> record) and Models3.txt ("M id slot file.mdl", then
// "A n file.ani") are next to it, every animation is also shown on its model: "<model>/<animation>.gltf", the
// animated glTF of the model with that animation alone (only when the animation fits the limbs of the model).

#include <cstdio>
#include <filesystem>
#include <map>
#include <set>

#include "../codecs/RequiemModel.h"
#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t MaxRecords = 1000000;

class RequiemAnimArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        std::string aryPath;
        std::string binPath;
        if (!Locate(stream.Path(), aryPath, binPath))
            return false;
        std::vector<Record> records;
        return ReadIndex(aryPath, Reader(binPath).Size(), records);
    }

    void Parse() override
    {
        std::string aryPath;
        std::string binPath;
        if (!Locate(_stream->Path(), aryPath, binPath))
            Fail(DUDI7_ERR_NOTFORMAT, "The .ARY/.BIN pair is incomplete");
        _stream = std::make_unique<Reader>(binPath);
        if (!ReadIndex(aryPath, _stream->Size(), _records))
            Fail(DUDI7_ERR_NOTFORMAT, "Not a Requiem animation pack");
        _formatId = "Requiem ANIM";
        _dirSeparator = "";
        char name[32];
        for (size_t i = 0; i < _records.size(); ++i)
        {
            std::snprintf(name, sizeof name, "anim_%03zu.bin", i);
            Add(name, _records[i].Offset, _records[i].Size);
        }
        AddModels(aryPath);
        if (!_shown.empty())
        {
            _dirSeparator = "/";
            _extractInternal = true; // the glTF files are made here
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        if (index < static_cast<int64_t>(_records.size()))
            return Archive::Extract(index, sink);
        const Shown &shown = _shown[static_cast<size_t>(index) - _records.size()];
        const std::string gltf = _builders[shown.Builder]->Build({{shown.Label, RecordBytes(shown.Record)}});
        if (gltf.empty())
        {
            LastError = "The model cannot be built";
            return DUDI7_ERR_IO;
        }
        return ExtractBytes(gltf.data(), static_cast<int64_t>(gltf.size()), sink);
    }

  private:
    struct Record
    {
        int64_t Offset;
        int64_t Size;
    };
    // A glTF entry: an animation shown on its model
    struct Shown
    {
        size_t Builder;
        size_t Record;
        std::string Label;
    };

    std::vector<Record> _records;
    std::vector<std::unique_ptr<RequiemModel::GltfBuilder>> _builders; // one per model: its mesh is built once
    std::vector<Shown> _shown;                                         // entries after the records

    auto RecordBytes(size_t record) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> bytes(static_cast<size_t>(_records[record].Size));
        _stream->Seek(_records[record].Offset);
        if (!bytes.empty())
            _stream->ReadBuffer(bytes.data(), bytes.size());
        return bytes;
    }

    // The glTF entries; nothing here can fail the archive: a model or an animation that does not fit is left out.
    // The glTF is built once to know its size, then again when it is extracted (it is the same: nothing is random),
    // so that each handle does not keep hundreds of them in memory.
    void AddModels(const std::string &aryPath)
    {
        using namespace RequiemModel;
        const std::string tabPath = Sibling(aryPath, ".tab");
        if (tabPath.empty())
            return;
        const std::vector<uint16_t> table = ReadAnimTable(tabPath);
        namespace fs = std::filesystem;
        const std::string folder = fs::path(std::u8string(aryPath.begin(), aryPath.end())).parent_path().string();
        const std::vector<ModelEntry> entries = ReadModels3(folder);
        std::map<std::string, size_t> builderOf;              // model file (lower case) -> builder
        std::vector<size_t> limbsOf;                          // limbs of the model of each builder
        std::map<std::string, std::set<std::string>> labelsOf; // model name -> labels used in its folder
        for (const ModelEntry &e : entries)
        {
            if (e.Kind != 'M' || e.Animations.empty()) // AnimFile.tab is indexed by the ids of the M models
                continue;
            try
            {
                const std::string file = ToLower(e.File);
                const std::string path = FindInFolder(folder, e.File);
                if (path.empty())
                    continue;
                // named as the file is (not as Models3.txt writes it): the glTF is then the one of the model file
                const std::string modelName = ChangeFileExt(FileNameOf(path), "");
                Model m;
                if (builderOf.find(file) == builderOf.end())
                {
                    Reader r(path);
                    if (!ParseSkeletal(r, m))
                        continue;
                }
                for (const auto &[number, animationFile] : e.Animations)
                {
                    const auto record = AnimRecordOf(table, e.Id, number);
                    if (!record || *record >= _records.size())
                        continue;
                    const std::vector<uint8_t> bytes = RecordBytes(*record);
                    const auto known = builderOf.find(file);
                    if (!AnimationFits(bytes, known != builderOf.end() ? limbsOf[known->second] : m.SectionCodes.size()))
                        continue;
                    size_t builder;
                    if (known != builderOf.end())
                        builder = known->second;
                    else
                    {
                        SlotsOf(entries, e.File, m.ArtSlot, m.OtherSlots);
                        _builders.push_back(std::make_unique<GltfBuilder>(m, modelName));
                        limbsOf.push_back(m.SectionCodes.size());
                        builder = builderOf[file] = _builders.size() - 1;
                    }
                    // the same label twice in a folder (a name listed twice, or two M lines of one model)
                    std::string label = ChangeFileExt(FileNameOf(animationFile), "");
                    auto &used = labelsOf[ToLower(modelName)];
                    if (used.count(ToLower(label)) != 0)
                        label += "_" + std::to_string(e.Id) + "_" + std::to_string(number);
                    if (used.count(ToLower(label)) != 0)
                        continue;
                    const std::string gltf = _builders[builder]->Build({{label, bytes}}); // empty if the skeleton cannot be built
                    if (gltf.empty())
                        continue;
                    used.insert(ToLower(label));
                    Add(modelName + "/" + label + ".gltf", 0, static_cast<int64_t>(gltf.size()));
                    _shown.push_back({builder, *record, label});
                }
            }
            catch (const std::exception &)
            {
                continue; // a model file that cannot be read
            }
        }
    }

    static auto Locate(const std::string &path, std::string &ary, std::string &bin) -> bool
    {
        const std::string ext = ToUpper(ExtensionOf(path));
        if (ext == ".ARY")
        {
            ary = path;
            bin = Sibling(path, ".bin");
        }
        else if (ext == ".BIN")
        {
            bin = path;
            ary = Sibling(path, ".ary");
        }
        return !ary.empty() && !bin.empty();
    }

    // The records of the index; false unless they follow one another and end where the .BIN ends
    static auto ReadIndex(const std::string &aryPath, int64_t binSize, std::vector<Record> &records) -> bool
    {
        Reader ary(aryPath);
        if (ary.Size() < 20)
            return false;
        const uint32_t count = ary.U32();
        if (count == 0 || count > MaxRecords || ary.Size() != 4 + static_cast<int64_t>(count) * 16)
            return false;
        int64_t expected = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t offset = ary.U32();
            const uint32_t zero = ary.U32();
            const uint32_t size = ary.U32();
            ary.U32();
            if (offset != expected || zero != 0)
                return false;
            expected += size;
            records.push_back({offset, size});
        }
        return expected == binSize;
    }
};
} // namespace

void RegisterRequiemAnim()
{
    Register<RequiemAnimArchive>("*.ARY;*.BIN", "Requiem: Avenging Angel animations (*.ARY;*.BIN)");
}
} // namespace Dud
