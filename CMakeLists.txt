cmake_minimum_required(VERSION 3.16)
project(RAND CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Der Klangkern – plattformunabhängig, wird von FM-1-Firmware und VST gemeinsam genutzt
add_library(rand_core STATIC
    core/rand_params.cpp
    core/rand_voice.cpp
    core/rand_engine.cpp
    core/rand_presets.cpp
)
target_include_directories(rand_core PUBLIC core)

# Demo-Renderer zum Probehören am PC
add_executable(render_demo tools/render_demo.cpp)
target_link_libraries(render_demo rand_core)
