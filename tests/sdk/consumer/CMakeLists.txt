# ============================================================================
# Consumer smoke (S33–34): отдельный CMake-проект, который видит ТОЛЬКО
# установленный SDK через find_package(RetroPolyEngine CONFIG REQUIRED).
#
#   cmake -S tests/sdk/consumer -B build/consumer \
#         -DCMAKE_PREFIX_PATH=/path/to/sdk \
#         -DRETRO_SDK_DIR=/path/to/sdk
# ============================================================================
cmake_minimum_required(VERSION 3.25)
project(retro_engine_sdk_consumer LANGUAGES CXX)

find_package(RetroPolyEngine CONFIG REQUIRED)

add_executable(sdk_probe main.cpp)
target_compile_features(sdk_probe PRIVATE cxx_std_14)
target_link_libraries(sdk_probe PRIVATE RetroPolyEngine::retro_engine)

# Сборка установленных примеров из SDK (без дерева исходников движка).
if(RETRO_SDK_DIR)
    foreach(_example hello_triangle rotating_cube)
        if(EXISTS "${RETRO_SDK_DIR}/examples/${_example}/CMakeLists.txt")
            add_subdirectory("${RETRO_SDK_DIR}/examples/${_example}"
                             "${CMAKE_CURRENT_BINARY_DIR}/${_example}_sdk")
        endif()
    endforeach()
endif()

# Проверка публичных целей SDK (наличие и namespaced-имена).
foreach(_rpe_target RetroPolyEngine::retro_engine RetroPolyEngine::hal
                    RetroPolyEngine::log)
    if(NOT TARGET ${_rpe_target})
        message(FATAL_ERROR "SDK consumer: отсутствует target ${_rpe_target}")
    endif()
endforeach()
