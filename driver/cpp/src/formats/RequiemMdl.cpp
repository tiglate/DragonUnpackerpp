// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Requiem: Avenging Angel models (Models/*.mdl): see codecs/RequiemModel.cpp for the layouts. Shown as Wavefront OBJ +
// MTL in the pose of the first frame of the first animation; every model is also given as a glTF 2.0 file (a
// skeletal model with its animations, a static one as a single limb) that the interface shows textured and animated.

#include <cstdio>
#include <filesystem>

#include "../codecs/RequiemModel.h"
#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
using namespace RequiemModel;

class RequiemMdlArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        Model m;
        return ParseStatic(stream, m) || (m = Model(), ParseSkeletal(stream, m));
    }

    void Parse() override
    {
        Model m;
        const bool isStatic = ParseStatic(*_stream, m);
        if (!isStatic)
        {
            m = Model();
            if (!ParseSkeletal(*_stream, m))
                Fail(DUDI7_ERR_NOTFORMAT, "Not a Requiem model");
        }
        _formatId = isStatic ? "Requiem MDL" : "Requiem MDL (skeletal)";
        _dirSeparator = "";
        _extractInternal = true;
        const std::string base = ChangeFileExt(FileNameOf(_stream->Path()), "");
        std::string gltf;
        if (isStatic)
        {
            // a static model is a skeleton of one limb without animation, so the viewer can show it with its texture
            namespace fs = std::filesystem;
            const std::string path = _stream->Path();
            SlotsOf(ReadModels3(Utf8Of(PathOf(path).parent_path())), FileNameOf(path), m.ArtSlot,
                    m.OtherSlots);
            Model one = m;
            one.SectionCodes = {0};
            one.LimbVertices = {static_cast<uint32_t>(m.Vertices.size())};
            gltf = MakeGltf(one, {}, base);
        }
        else
        {
            const std::vector<Animation> animations = CollectAnimations(m, _stream->Path(), m.ArtSlot, m.OtherSlots);
            if (!animations.empty())
            {
                gltf = MakeGltf(m, animations, base);
                PoseModel(m, animations[0]);
            }
        }
        const std::string obj = MakeObj(m, base);
        std::string mtl = "newmtl texture\nKd 1 1 1\n";
        if (m.ArtSlot >= 0)
        {
            // the game loads the picture from this slot of the art archive (the D3D set has the colors)
            char slot[64];
            std::snprintf(slot, sizeof slot, "tex_%04d.tga", m.ArtSlot);
            mtl += "# texture: slot " + std::to_string(m.ArtSlot) + " of the art archive (" + m.Texture + " is only a label)\n";
            mtl += std::string("map_Kd ") + slot + "\n";
        }
        else if (!m.Texture.empty())
            mtl += "map_Kd " + m.Texture + "\n";
        _files.emplace_back(obj.begin(), obj.end());
        _files.emplace_back(mtl.begin(), mtl.end());
        Add(base + ".obj", 0, static_cast<int64_t>(_files[0].size()));
        Add(base + ".mtl", 0, static_cast<int64_t>(_files[1].size()));
        if (!gltf.empty())
        {
            _files.emplace_back(gltf.begin(), gltf.end());
            Add(base + ".gltf", 0, static_cast<int64_t>(_files[2].size()));
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const auto &bytes = _files[static_cast<size_t>(index)];
        return ExtractBytes(bytes.data(), static_cast<int64_t>(bytes.size()), sink);
    }

  private:
    std::vector<std::vector<uint8_t>> _files;
};
} // namespace

void RegisterRequiemMdl()
{
    Register<RequiemMdlArchive>("*.MDL", "Requiem: Avenging Angel models (*.MDL)");
}
} // namespace Dud
