# Vendored: renderdoc_app.h

Source: <https://github.com/baldurk/renderdoc/blob/v1.7.0/renderdoc/app/renderdoc_app.h>

## Why vendored

The RenderDoc in-application API is consumed **at runtime** via `dlopen` /
`GetModuleHandle` + `dlsym` / `GetProcAddress`. There is no link-time
dependency on `librenderdoc.so` / `renderdoc.dll`, but the header must be
present at build time to resolve the function-pointer struct.

## How to upgrade

```sh
curl -L https://raw.githubusercontent.com/baldurk/renderdoc/vX.Y.Z/renderdoc/app/renderdoc_app.h \
     -o renderdoc_app.h
```

Then update the version requested in `src/wsl/gfx/renderdoc.cpp` (`init()`)
to match the new header's `RENDERDOC_Version` enum and any new fields on
the API struct.

## Why not CPM

`renderdoc_app.h` is a single header with no transitive includes. CPM is
overkill and the file is small enough to vendor directly. A network fetch
on every CMake configure would also slow the build.
