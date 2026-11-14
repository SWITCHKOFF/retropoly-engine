# ============================================================================
# RetroPolyEngine project template (S33–34).
#
# Скопируйте каталог в новый проект и соберите против установленного SDK:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/retro-engine-sdk
#   cmake --build build
# ============================================================================
cmake_minimum_required(VERSION 3.25)
project(retro_project LANGUAGES CXX)

find_package(RetroPolyEngine CONFIG REQUIRED)

add_executable(retro_project src/main.cpp)
target_compile_features(retro_project PRIVATE cxx_std_14)
target_link_libraries(retro_project PRIVATE RetroPolyEngine::retro_engine)

if(WIN32)
    set_target_properties(retro_project PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
