# Dear ImGui 1.92.9b

Bundled from the user-supplied `imgui-1.92.9b.zip`. The library sources and
headers are unmodified; see `imgui/LICENSE.txt`. CMake builds the four core
translation units. `imgui_demo.cpp` is retained as an upstream reference,
not linked into Quanta. No network access, fetch script, submodule,
OpenGL/DirectX SDK, SDL, or GLFW is needed.

Quanta owns its native Win32/GDI and XCB window backends and a CPU renderer
with the 1.92 dynamic texture protocol. Unused upstream backend/example
projects, CI scripts and external font files are not part of this subset.

## Upstream file hashes

```text
173506a2d6f7fb67990d257fb2507f188690eca39060c39469ae7bef43aae2a3  LICENSE.txt
cce505252477d754005f7c8931b26589db50fea2e24011294f19c1bd9f21accf  docs/BACKENDS.md
0c9c57f9a59256ed6f5c8d8f43596d63747f6c9c96c92c617482cf43c832409b  docs/CHANGELOG.txt
7f6c6e61ac8d4d70ffabe3357ddceba57a449abd2fd279073224ec97a2357918  docs/FONTS.md
1abb04cbd7419abf31de079a619a9c1e50442d39abc5747b41beee8575da93ff  docs/README.md
5755e1b8d6ab0d7811a9d7cac0f509878b51fc5ee77e09bd0f235bcb414971e7  imconfig.h
5a1e5128c0305f50b6556a77f71fde35836505883b74c1d6ae44723243f1e8de  imgui.cpp
0d8db1045db01d908853adfd26ae07c5bc5ab4789d4515f6ea34234a69ade0ca  imgui.h
8930ec6bb26844a3d24aa519a07f674ec666b8c22c4d8a74fa6cc128033ed494  imgui_demo.cpp
83d30419a8e06a5f0a8692ee6de186f7ecfddecdc33769e76244837e2975b400  imgui_draw.cpp
efba9bccc971cc49ec3da3168968ba412c7ea89745c104a7f5c004c8349bc71f  imgui_internal.h
b5deabe5b569ab712c11b6556562a646bf88327320fc60a451071b2a36498d72  imgui_tables.cpp
a8a0b2f65caa5711467d9e3ca4028fded48329ea65e11ec92a968293dc2aa231  imgui_widgets.cpp
889b396795202d1457560a797a7242e96f6f132d4b88ca2d69be58bf05e1771f  imstb_rectpack.h
a985f5fa0ed97353d493b497961e9eef52082edcd045cf6954b69990ec9d0741  imstb_textedit.h
c51a0f7e7ea760f2366bd3752635ec58e21fccfec4a832501639990ba6ce0528  imstb_truetype.h
```
