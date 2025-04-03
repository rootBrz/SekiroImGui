cmake_minimum_required(VERSION 3.10)
project(SekiroFpsUnlockImGui CXX)

if(CMAKE_HOST_WIN32)
  set(SEKIRO_PATH "E:/SteamLibrary/steamapps/common/Sekiro")
  find_library(D3DCOMPILER_LIB NAMES d3dcompiler_47 d3dcompiler PATHS C:/msys64/ucrt64/lib)
  find_library(DWMAPI_LIB NAMES dwmapi PATHS C:/msys64/ucrt64/lib)
else()
  set(CMAKE_AR "${CMAKE_C_COMPILER}-ar")
  set(CMAKE_NM "${CMAKE_C_COMPILER}-nm")
  set(CMAKE_RANLIB "${CMAKE_C_COMPILER}-ranlib")
  set(SEKIRO_PATH "/run/media/alan/games/SteamLibrary/steamapps/common/Sekiro")
  find_library(D3DCOMPILER_LIB NAMES d3dcompiler_47 d3dcompiler PATHS usr/x86_64-w64-mingw32/sys-root/mingw/lib)
  find_library(DWMAPI_LIB NAMES dwmapi PATHS usr/x86_64-w64-mingw32/sys-root/mingw/lib)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fwhole-program -O3 -flto -ffat-lto-objects -ffunction-sections -fdata-sections -fomit-frame-pointer -fvisibility=hidden")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ImGui)
file(GLOB IMGUI_SOURCES
    "${IMGUI_DIR}/*.cpp"
    "${IMGUI_DIR}/imgui_impl_dx11.cpp"
    "${IMGUI_DIR}/imgui_impl_win32.cpp"
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${IMGUI_DIR})
 
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/MinHook)

file(GLOB SOURCE_FILES *.cpp *.h)
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

target_link_options(${PROJECT_NAME} PRIVATE -Wl,--gc-sections)

target_link_libraries(${PROJECT_NAME}
    PRIVATE 
    -static
    dinput8
    d3d11
    imgui 
    minhook
    ${D3DCOMPILER_LIB}
    ${DWMAPI_LIB}
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_STRIP} $<TARGET_FILE:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:${PROJECT_NAME}>
        "${SEKIRO_PATH}/dinput8.dll"
    COMMENT "Copying DLL to Sekiro directory"
)