cmake_minimum_required(VERSION 3.21)
project(Flip3DComp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Flip3DComp WIN32
    src/main.cpp
    src/Flip3DComp.cpp
    src/Flip3DComp_Window.cpp
    src/Flip3DComp_Cards.cpp
    src/Flip3DComp_Composition.cpp
    src/Flip3DComp_Update.cpp
    src/Flip3DComp_Rotation.cpp
    src/Flip3DComp_Input.cpp
    src/Flip3DComp_View.cpp
    src/Flip3DComp_ShellHook.cpp
    src/Flip3DComp_Accessible.cpp
    src/Flip3DAccessible.cpp
    src/Timeline.cpp
    src/WindowCapture.cpp   # Stage 2: WGC capture bridge (D3D11 card rendering)
)

target_include_directories(Flip3DComp PRIVATE src)
target_link_libraries(Flip3DComp
    dwmapi
    dcomp
    dxgi
    d3d11         # Stage 2: now also does real MSAA card rendering, not just DComp interop surfaces
    d3dcompiler   # Stage 2: HLSL shader compilation
    d2d1          # Stage 2: WindowCapture's InteropCompositor bridge
    runtimeobject # Stage 2: RoInitialize / RoGetActivationFactory
    windowsapp    # Stage 2: WinRT projected ABI (Windows.Graphics.Capture)
    user32
    gdi32
    oleacc
)
