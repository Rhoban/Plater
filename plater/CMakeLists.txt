cmake_minimum_required(VERSION 3.6)

project(plater)

if(MSVC)
# Native MSVC build use static C runtime
foreach(flag_var
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL
        CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL
        CMAKE_CXX_FLAGS_RELWITHDEBINFO)
  string(REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
endforeach(flag_var)
else()
# strip release build
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
endif()

find_package(Threads)

set(ALL_SOURCES
    stl/Point3.cpp
    stl/Face.cpp
    stl/Volume.cpp
    stl/Model.cpp
    stl/StlFactory.cpp
 
    Bitmap.cpp
    PlacedPart.cpp
    Placer.cpp
    Plate.cpp
    Part.cpp
    QuadTree.cpp
    Rectangle.cpp
    Request.cpp
    Solution.cpp
    Triangle.cpp
    ThreeMF.cpp
    Zip.cpp

    sleep.cpp
    log.cpp
    util.cpp
    wingetopt.cpp
)

add_library(libplater STATIC ${ALL_SOURCES})
if(MSVC)
  target_compile_options(libplater PRIVATE /wd4244 /wd4267 /wd4305 /wd4307 /wd4996)
else()
  target_compile_options(libplater PRIVATE -std=c++11 -Wall -pedantic -Wno-reorder)
endif()

target_include_directories(libplater PUBLIC .)
target_link_libraries(libplater PRIVATE Threads::Threads)

add_executable(plater main.cpp plater.manifest)
if(MSVC)
  target_compile_options(plater PRIVATE /wd4244)
endif()
if(MINGW)
  target_link_options(plater PRIVATE "-static")
endif()
target_link_libraries(plater libplater)

install(TARGETS plater RUNTIME DESTINATION bin)

# Prototype: bit-packed collision benchmark (not installed)
add_executable(bench_overlap bench_overlap.cpp)
if(NOT MSVC)
  target_compile_options(bench_overlap PRIVATE -std=c++11 -O2)
endif()
target_link_libraries(bench_overlap libplater)
