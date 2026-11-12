cmake_minimum_required(VERSION 3.20)

project(orbduel VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# SDL2：离线预编译包，按编译器自动选（ADR-013）
#   mingw: include/SDL2, lib/*.dll.a, bin/SDL2.dll
#   msvc : include 扁平, lib/*.lib,  bin/*.dll
# ---------------------------------------------------------------------------
if(MSVC)
    set(SDL2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sdl2/msvc")
    set(SDL2_INCLUDE_DIR "${SDL2_ROOT}/include")
    set(SDL2_LINK_LIBS
        "${SDL2_ROOT}/lib/SDL2main.lib"
        "${SDL2_ROOT}/lib/SDL2.lib"
        "${SDL2_ROOT}/lib/SDL2_image.lib"
        "${SDL2_ROOT}/lib/SDL2_ttf.lib")
else()
    set(SDL2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sdl2/mingw")
    set(SDL2_INCLUDE_DIR "${SDL2_ROOT}/include/SDL2")
    set(SDL2_LINK_LIBS
        mingw32
        "${SDL2_ROOT}/lib/libSDL2main.a"
        "${SDL2_ROOT}/lib/libSDL2.dll.a"
        "${SDL2_ROOT}/lib/libSDL2_image.dll.a"
        "${SDL2_ROOT}/lib/libSDL2_ttf.dll.a")
endif()

foreach(lib ${SDL2_LINK_LIBS})
    if(IS_ABSOLUTE "${lib}" AND NOT EXISTS "${lib}")
        message(FATAL_ERROR
            "找不到 SDL2 预编译库: ${lib}\n"
            "检查 third_party/sdl2 是否完整。")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# 引擎静态库
# ---------------------------------------------------------------------------
add_library(orbduel_engine STATIC
    src/compile_check.cpp
    src/core/Event.cpp
    src/core/Log.cpp
    src/core/Rng.cpp
    src/core/Time.cpp
    src/core/version.cpp
    src/physics/Arena.cpp
    src/physics/Collision.cpp
    src/physics/World.cpp
    src/render/AssetCache.cpp
    src/render/Fx.cpp
    src/render/Renderer.cpp
    src/content/ContentLoader.cpp
    src/match/Match.cpp
    src/match/MatchRule.cpp
    src/match/Skill.cpp
    src/match/StallGuard.cpp
    src/match/Zone.cpp
    src/game/Menu.cpp
    src/game/Game.cpp
)

target_include_directories(orbduel_engine PUBLIC include)
target_include_directories(orbduel_engine PRIVATE
    "${SDL2_INCLUDE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
target_link_libraries(orbduel_engine PRIVATE ${SDL2_LINK_LIBS})

if(MSVC)
    target_compile_options(orbduel_engine PRIVATE /W3 /utf-8)
else()
    target_compile_options(orbduel_engine PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# 可执行文件
# ---------------------------------------------------------------------------
add_executable(orbduel src/game/main.cpp)
target_link_libraries(orbduel PRIVATE orbduel_engine ${SDL2_LINK_LIBS})
target_include_directories(orbduel PRIVATE
    "${SDL2_INCLUDE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

if(MSVC)
    target_compile_options(orbduel PRIVATE /W3 /utf-8)
else()
    target_compile_options(orbduel PRIVATE -Wall -Wextra)
endif()

# 所有配置都输出到 build/bin，方便脚本与文档统一路径
foreach(cfg "" _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
    set_target_properties(orbduel PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY${cfg} "${CMAKE_BINARY_DIR}/bin")
endforeach()

# SDL2 是动态链接：把 DLL 拷到 exe 旁边
file(GLOB SDL2_DLLS "${SDL2_ROOT}/bin/*.dll")
foreach(dll ${SDL2_DLLS})
    add_custom_command(TARGET orbduel POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${dll}" "$<TARGET_FILE_DIR:orbduel>")
endforeach()
