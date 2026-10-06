# Animation example assets

Rigged sample asset used by the ozz-animation integration tests
(`tests/weasel-core/test_ozz_loader.cpp`) and as a reference for the
`weasel-cli import anim` workflow. See `OZZ_ANIMATION_PLAN.md` (M2).

## Contents

| File | Source |
|---|---|
| `rsc/models/Fox.glb` | Khronos glTF-Sample-Assets, model **Fox** (glTF-Binary) |
| `rsc/models/Fox.skel.ozz` | Converted by `gltf2ozz` (runtime skeleton) |
| `rsc/models/Fox_*.anim.ozz` | Converted by `gltf2ozz` (Survey, Walk, Run clips) |

## Regenerating the ozz files

```bash
weasel-cli import anim rsc/models/Fox.glb
```

Or invoke the tool directly:

```bash
gltf2ozz --file=rsc/models/Fox.glb \
  --config='{"skeleton":{"filename":"rsc/models/Fox.skel.ozz"},
             "animations":[{"clip":"*","filename":"rsc/models/Fox_*.anim.ozz"}]}'
```

## License

Fox model files are licensed under **CC0-1.0 / CC-BY-4.0** per the upstream
[license file](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/Fox/LICENSE.md).
Source: [KhronosGroup/glTF-Sample-Assets](https://github.com/KhronosGroup/glTF-Sample-Assets).
