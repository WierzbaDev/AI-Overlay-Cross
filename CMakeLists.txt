cmake_minimum_required(VERSION 3.21)

# ============================================================================
# Project declaration
# OBJCXX is enabled conditionally below via enable_language(OBJCXX).
# Listing only CXX here is correct — enable_language() can be called after
# project() and before any targets are defined.
# ============================================================================
project(ai_overlay_cross
    VERSION 1.0.0
    DESCRIPTION "AI Desktop Overlay — Cross-Platform C++ Core (macOS + Windows)"
    LANGUAGES CXX
)

# ============================================================================
# Platform detection
# ============================================================================
if(APPLE)
    set(PLATFORM_MACOS    TRUE)
    set(PLATFORM_WINDOWS  FALSE)
    enable_language(OBJCXX)
elseif(WIN32)
    set(PLATFORM_MACOS    FALSE)
    set(PLATFORM_WINDOWS  TRUE)
else()
    message(FATAL_ERROR
        "Unsupported platform. Only macOS and Windows are supported.")
endif()

# ============================================================================
# Language standards
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(PLATFORM_MACOS)
    set(CMAKE_OBJCXX_STANDARD 17)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
    set(CMAKE_OBJCXX_EXTENSIONS OFF)
endif()

# ============================================================================
# Build type default (single-config generators: Makefile, Ninja)
# ============================================================================
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
        STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# ============================================================================
# Third-party dependencies
# Prefer package-manager supplied CMake packages (vcpkg, Homebrew, etc.),
# but fall back to standard include locations for header-only libraries so a
# macOS build does not depend on a specific registry state.
# ============================================================================
function(require_header_only_target target_name header_name)
    if(TARGET "${target_name}")
        return()
    endif()

    string(REPLACE "::" "_" target_var "${target_name}")
    string(TOUPPER "${target_var}" target_var_upper)
    set(formula_name "${ARGV2}")

    set(search_paths
        /opt/homebrew/include
        /usr/local/include
    )

    if(formula_name)
        list(APPEND search_paths
            "/opt/homebrew/opt/${formula_name}/include"
            "/usr/local/opt/${formula_name}/include"
        )
    endif()

    find_path("${target_var_upper}_INCLUDE_DIR"
        NAMES "${header_name}"
        PATHS ${search_paths}
    )

    if(NOT EXISTS "${${target_var_upper}_INCLUDE_DIR}/${header_name}")
        message(FATAL_ERROR
            "Missing dependency for ${target_name}: could not find header "
            "${header_name}. Install it with your package manager or provide "
            "it via CMAKE_PREFIX_PATH/CMAKE_INCLUDE_PATH.")
    endif()

    add_library("${target_name}" INTERFACE IMPORTED)
    set_target_properties("${target_name}" PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${${target_var_upper}_INCLUDE_DIR}"
    )
endfunction()

find_package(websocketpp   CONFIG QUIET)
find_package(asio          CONFIG QUIET)
find_package(nlohmann_json CONFIG QUIET)
find_package(Boost         QUIET)

require_header_only_target(websocketpp::websocketpp "websocketpp/server.hpp" "websocketpp")
require_header_only_target(nlohmann_json::nlohmann_json "nlohmann/json.hpp" "nlohmann-json")

set(AI_OVERLAY_USE_STANDALONE_ASIO FALSE)
if(TARGET asio::asio)
    set(AI_OVERLAY_USE_STANDALONE_ASIO TRUE)
else()
    if(TARGET Boost::headers)
        set(AI_OVERLAY_ASIO_FALLBACK_TARGET Boost::headers)
    elseif(TARGET Boost::boost)
        set(AI_OVERLAY_ASIO_FALLBACK_TARGET Boost::boost)
    else()
        find_path(AI_OVERLAY_BOOST_INCLUDE_DIR
            NAMES boost/asio.hpp
            PATHS
                /opt/homebrew/include
                /usr/local/include
                /opt/homebrew/opt/boost/include
                /usr/local/opt/boost/include
        )
        if(EXISTS "${AI_OVERLAY_BOOST_INCLUDE_DIR}/boost/asio.hpp")
            add_library(ai_overlay_boost_headers INTERFACE IMPORTED)
            set_target_properties(ai_overlay_boost_headers PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${AI_OVERLAY_BOOST_INCLUDE_DIR}"
            )
            set(AI_OVERLAY_ASIO_FALLBACK_TARGET ai_overlay_boost_headers)
        else()
            find_path(AI_OVERLAY_STANDALONE_ASIO_INCLUDE_DIR
                NAMES asio.hpp
                PATHS
                    /opt/homebrew/include
                    /usr/local/include
                    /opt/homebrew/opt/asio/include
                    /usr/local/opt/asio/include
            )
            if(EXISTS "${AI_OVERLAY_STANDALONE_ASIO_INCLUDE_DIR}/asio.hpp")
                add_library(asio::asio INTERFACE IMPORTED)
                set_target_properties(asio::asio PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${AI_OVERLAY_STANDALONE_ASIO_INCLUDE_DIR}"
                )
                set(AI_OVERLAY_USE_STANDALONE_ASIO TRUE)
            else()
                message(FATAL_ERROR
                    "Missing Asio backend: could not find standalone Asio or Boost.Asio headers.")
            endif()
        endif()
    endif()
endif()

# ============================================================================
# Absolute source root variables — avoids relative-path problems with
# CLion, Xcode, or any generator that sets a non-source CWD.
# ============================================================================
set(SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ============================================================================
# Shared sources (compiled on both platforms)
# ============================================================================
set(SHARED_SOURCES
    "${SRC_ROOT}/shared/StateManager.cpp"
    "${SRC_ROOT}/shared/WebSocketServer.cpp"
)
set(SHARED_HEADERS
    "${INC_ROOT}/Common.h"
    "${INC_ROOT}/MessageQueue.h"
    "${INC_ROOT}/StateManager.h"
    "${INC_ROOT}/IOverlayWindow.h"
    "${INC_ROOT}/IHotkeyManager.h"
    "${INC_ROOT}/WebSocketServer.h"
)

# ============================================================================
# macOS platform sources
# ============================================================================
if(PLATFORM_MACOS)
    set(PLATFORM_SOURCES
        "${SRC_ROOT}/platform/mac/main_mac.mm"
        "${SRC_ROOT}/platform/mac/AppController_mac.mm"
        "${SRC_ROOT}/platform/mac/OverlayWindow_mac.mm"
        "${SRC_ROOT}/platform/mac/HotkeyManager_mac.mm"
    )
    set(PLATFORM_HEADERS
        "${INC_ROOT}/AppController_mac.h"
        "${INC_ROOT}/OverlayWindow_mac.h"
        "${INC_ROOT}/HotkeyManager_mac.h"
    )

    # Tag .mm files as OBJCXX BEFORE add_executable so CMake's source scanner
    # picks up the correct compiler for each translation unit.
    # Absolute paths are required here — relative paths are resolved against
    # CMAKE_CURRENT_SOURCE_DIR at the time of the call, which may differ in
    # multi-dir builds (CLion, Xcode, etc.).
    set_source_files_properties(
        "${SRC_ROOT}/platform/mac/main_mac.mm"
        "${SRC_ROOT}/platform/mac/AppController_mac.mm"
        "${SRC_ROOT}/platform/mac/OverlayWindow_mac.mm"
        "${SRC_ROOT}/platform/mac/HotkeyManager_mac.mm"
        PROPERTIES LANGUAGE OBJCXX
    )
endif()

# ============================================================================
# Windows platform sources
# ============================================================================
if(PLATFORM_WINDOWS)
    set(PLATFORM_SOURCES
        "${SRC_ROOT}/platform/win/main_win.cpp"
        "${SRC_ROOT}/platform/win/AppController_win.cpp"
        "${SRC_ROOT}/platform/win/OverlayWindow_win.cpp"
        "${SRC_ROOT}/platform/win/HotkeyManager_win.cpp"
    )
    set(PLATFORM_HEADERS
        "${INC_ROOT}/AppController_win.h"
        "${INC_ROOT}/OverlayWindow_win.h"
        "${INC_ROOT}/HotkeyManager_win.h"
    )
endif()

# ============================================================================
# Executable target
# ============================================================================
if(PLATFORM_WINDOWS)
    # WIN32 keyword = linker uses wWinMain, no console window allocated
    add_executable(ai_overlay WIN32
        ${SHARED_SOURCES}   ${SHARED_HEADERS}
        ${PLATFORM_SOURCES} ${PLATFORM_HEADERS}
    )
else()
    add_executable(ai_overlay
        ${SHARED_SOURCES}   ${SHARED_HEADERS}
        ${PLATFORM_SOURCES} ${PLATFORM_HEADERS}
    )
endif()

# ============================================================================
# Include directories
# ============================================================================
target_include_directories(ai_overlay
    PRIVATE "${INC_ROOT}"
)

# ============================================================================
# Compile definitions — shared
# ============================================================================
target_compile_definitions(ai_overlay
    PRIVATE
        _WEBSOCKETPP_CPP11_THREAD_
        _WEBSOCKETPP_CPP11_FUNCTIONAL_
        _WEBSOCKETPP_CPP11_SYSTEM_ERROR_
        _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
        _WEBSOCKETPP_CPP11_MEMORY_
)

if(AI_OVERLAY_USE_STANDALONE_ASIO)
    target_compile_definitions(ai_overlay PRIVATE
        AI_OVERLAY_USE_STANDALONE_ASIO
        ASIO_STANDALONE
        ASIO_ENABLE_OLD_SERVICES
    )
endif()

# ============================================================================
# macOS-specific configuration
# ============================================================================
if(PLATFORM_MACOS)
    target_compile_options(ai_overlay PRIVATE
        -Wall
        -Wextra
        -Wno-deprecated-declarations    # suppress ObjC API deprecation noise
        -Wno-unused-parameter
        $<$<CONFIG:Debug>:   -O0 -g3>
        $<$<CONFIG:Release>: -O2>
        $<$<CONFIG:RelWithDebInfo>: -O2 -g>
    )

    # Link frameworks with -framework flags directly.
    # find_library(FOO AppKit) stores the result in ${FOO} (not ${FOO_LIB}),
    # which is a common source of "undefined variable" link failures.
    # Using string flags avoids this entirely.
    target_link_libraries(ai_overlay
        PRIVATE
            "-framework AppKit"
            "-framework Carbon"
            "-framework CoreFoundation"
            "-framework ApplicationServices"
            websocketpp::websocketpp
            nlohmann_json::nlohmann_json
    )

    if(AI_OVERLAY_USE_STANDALONE_ASIO)
        target_link_libraries(ai_overlay PRIVATE asio::asio)
    else()
        target_link_libraries(ai_overlay PRIVATE "${AI_OVERLAY_ASIO_FALLBACK_TARGET}")
    endif()

    set_target_properties(ai_overlay PROPERTIES
        MACOSX_DEPLOYMENT_TARGET "11.0"
        MACOSX_BUNDLE            FALSE
    )
endif()

# ============================================================================
# Windows-specific configuration
# ============================================================================
if(PLATFORM_WINDOWS)
    target_compile_definitions(ai_overlay
        PRIVATE
            WINVER=0x0A00
            _WIN32_WINNT=0x0A00
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            UNICODE
            _UNICODE
            _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
    )

    if(MSVC)
        target_compile_options(ai_overlay PRIVATE
            /W4
            /WX-            # don't treat warnings as errors (third-party headers)
            /wd4996         # deprecated POSIX names (asio)
            /wd4127         # constant conditional expression (websocketpp)
            /wd4267         # size_t to int (websocketpp)
            /wd4244         # precision-loss conversions
            /wd4100         # unreferenced formal parameter
            /MP             # parallel compilation
            /permissive-    # strict C++ conformance
            $<$<CONFIG:Debug>:   /Od /Zi /RTC1>
            $<$<CONFIG:Release>: /O2 /Oi /GL>
        )
        target_link_options(ai_overlay PRIVATE
            $<$<CONFIG:Debug>:   /DEBUG>
            $<$<CONFIG:Release>: /LTCG>
        )
    endif()

    target_link_libraries(ai_overlay
        PRIVATE
            websocketpp::websocketpp
            nlohmann_json::nlohmann_json
            user32 gdi32 kernel32 dwmapi
            shell32 ole32 advapi32
            ws2_32 mswsock
    )

    if(AI_OVERLAY_USE_STANDALONE_ASIO)
        target_link_libraries(ai_overlay PRIVATE asio::asio)
    else()
        target_link_libraries(ai_overlay PRIVATE "${AI_OVERLAY_ASIO_FALLBACK_TARGET}")
    endif()
endif()

# ============================================================================
# IDE grouping (Xcode / Visual Studio / CLion)
# ============================================================================
source_group("Headers/Shared"    FILES ${SHARED_HEADERS})
source_group("Sources/Shared"    FILES ${SHARED_SOURCES})
source_group("Sources/Platform"  FILES ${PLATFORM_SOURCES})
source_group("Headers/Platform"  FILES ${PLATFORM_HEADERS})

# ============================================================================
# Install
# ============================================================================
install(TARGETS ai_overlay RUNTIME DESTINATION bin)

# ============================================================================
# Summary
# ============================================================================
if(PLATFORM_MACOS)
    set(_PLAT "macOS")
else()
    set(_PLAT "Windows")
endif()
message(STATUS "")
message(STATUS "  ai_overlay_cross v${PROJECT_VERSION}")
message(STATUS "  Platform  : ${_PLAT}")
message(STATUS "  Build     : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler  : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  C++ std   : ${CMAKE_CXX_STANDARD}")
message(STATUS "")
