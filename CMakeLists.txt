cmake_minimum_required(VERSION 3.10)
project(SekiroFpsUnlockImGui C CXX)

set(VERSION_MAJOR 1)
set(VERSION_MINOR 5)
set(VERSION_PATCH 0)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SEKIRO_PATH $ENV{SEKIRO_PATH})
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

# ImGui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ImGui)
file(GLOB IMGUI_SOURCES
    "${IMGUI_DIR}/*.cpp"
    "${IMGUI_DIR}/imgui_impl_dx11.cpp"
    "${IMGUI_DIR}/imgui_impl_win32.cpp"
)
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${IMGUI_DIR})
 
# MinHook
set(MINHOOK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/MinHook)
file(GLOB MINHOOK_SOURCES
    "${MINHOOK_DIR}/src/*.c"
    "${MINHOOK_DIR}/src/hde/*.c"
)
add_library(minhook STATIC ${MINHOOK_SOURCES})
target_include_directories(minhook PUBLIC "${MINHOOK_DIR}/include")

# DLL Details
SET(CMAKE_RC_COMPILER "windres")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/version.rc @ONLY)
set(RC_FILE ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
set(RES_FILE ${CMAKE_CURRENT_BINARY_DIR}/version.res)
add_custom_command(OUTPUT ${RES_FILE}
    COMMAND ${CMAKE_RC_COMPILER} -O coff -i ${RC_FILE} -o ${RES_FILE}
    MAIN_DEPENDENCY ${RC_FILE}
)
set(VERSION_RESOURCE ${RES_FILE})

# Source files
file(GLOB SOURCE_FILES *.cpp *.h)
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

# DLL Details
set_source_files_properties(${VERSION_RESOURCE} PROPERTIES EXTERNAL_OBJECT TRUE)
target_sources(${PROJECT_NAME} PRIVATE ${VERSION_RESOURCE})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h @ONLY)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

target_compile_options(${PROJECT_NAME} PRIVATE -O3 -flto -ffunction-sections -fdata-sections)
target_link_options(${PROJECT_NAME} PRIVATE -Wl,--gc-sections)

target_link_libraries(${PROJECT_NAME}
    PRIVATE 
    -static
    d3d11
    dxgi
    imgui 
    minhook
    -lgdi32 -ldwmapi -ld3dcompiler
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_STRIP} $<TARGET_FILE:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:${PROJECT_NAME}>
        "${SEKIRO_PATH}/d3dcompiler_43.dll"
    COMMENT "Copying DLL to Sekiro directory"
)