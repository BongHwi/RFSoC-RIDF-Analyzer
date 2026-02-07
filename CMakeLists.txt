cmake_minimum_required(VERSION 3.16)
project(rfsoc_ridf_analyzer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED ENV{ROOTSYS})
    set(CMAKE_PREFIX_PATH "$ENV{ROOTSYS}" ${CMAKE_PREFIX_PATH})
elseif(DEFINED ROOT_DIR)
    set(CMAKE_PREFIX_PATH "${ROOT_DIR}" ${CMAKE_PREFIX_PATH})
else()
    set(CMAKE_PREFIX_PATH "/opt/root" ${CMAKE_PREFIX_PATH})
endif()

find_package(ROOT REQUIRED COMPONENTS Core RIO Net Hist Graf Graf3d Gpad Tree TreePlayer Rint MathCore Thread)
include(${ROOT_USE_FILE})

include_directories(${CMAKE_SOURCE_DIR}/include)

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    find_path(NLOHMANN_JSON_INCLUDE_DIR
        NAMES nlohmann/json.hpp
        PATHS
            /opt/homebrew/include
            /usr/local/include
            /usr/include
    )
    if(NOT NLOHMANN_JSON_INCLUDE_DIR)
        message(FATAL_ERROR "nlohmann/json.hpp not found. Install nlohmann-json (e.g. brew install nlohmann-json).")
    endif()
endif()

set(LIB_SOURCES
    src/ModuleAbst.cpp
    src/ModuleC16.cpp
    src/ModuleMADC.cpp
    src/ModuleV7XX.cpp
    src/ModuleV1290.cpp
    src/ModuleFIT.cpp
    src/RIDFParser.cpp
    src/RIDFPull.cpp
)

ROOT_GENERATE_DICTIONARY(G__ridfana
    RIDFParser.h RIDFPull.h
    ModuleAbst.h ModuleMADC.h ModuleV7XX.h
    ModuleFIT.h ModuleC16.h ModuleV1290.h
    LINKDEF ${CMAKE_SOURCE_DIR}/include/LinkDef.h
    OPTIONS -I${CMAKE_SOURCE_DIR}/include
)

add_library(ridfana SHARED ${LIB_SOURCES} G__ridfana.cxx)
target_link_libraries(ridfana PUBLIC ${ROOT_LIBRARIES})
set_target_properties(ridfana PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
)

add_executable(rfsoc_ridf_analyzer src/rfsoc_ridf_analyzer.cpp)
target_link_libraries(rfsoc_ridf_analyzer PRIVATE ridfana ${ROOT_LIBRARIES})
set_target_properties(rfsoc_ridf_analyzer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

add_executable(export_waveforms src/export_waveforms.cpp)
target_include_directories(export_waveforms PRIVATE ${ROOT_INCLUDE_DIRS})
target_link_libraries(export_waveforms ${ROOT_LIBRARIES})
set_target_properties(export_waveforms PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

add_executable(analyze_waveforms
    src/analyze_waveforms.cpp
    src/WaveformAnalysis.cpp
)
target_include_directories(analyze_waveforms PRIVATE
    ${ROOT_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)
if(nlohmann_json_FOUND)
    target_link_libraries(analyze_waveforms PRIVATE nlohmann_json::nlohmann_json)
else()
    target_include_directories(analyze_waveforms PRIVATE ${NLOHMANN_JSON_INCLUDE_DIR})
endif()
target_link_libraries(analyze_waveforms PRIVATE ${ROOT_LIBRARIES})
set_target_properties(analyze_waveforms PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

add_custom_command(TARGET ridfana POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/libridfana_rdict.pcm
        ${CMAKE_SOURCE_DIR}/lib/
)
