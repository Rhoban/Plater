cmake_minimum_required(VERSION 3.6)

project(plater-gui)

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

find_package(Qt5 COMPONENTS Widgets OpenGL REQUIRED)
find_package(OpenGL COMPONENTS OpenGL REQUIRED)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (NOT TARGET libplater)
  add_subdirectory(../plater plater)
endif()

add_executable(plater-gui WIN32
  main.cpp
  mainwindow.cpp
  partslist.cpp
  worker.cpp
  viewer.cpp
  wizard.cpp
  platesviewer.cpp
  about.cpp
  icons.qrc
  gui.rc
)

if(MSVC)
  # disable some warnings
  target_compile_options(plater-gui PRIVATE /wd4267 /wd4305 /wd4307)
endif()

target_link_libraries(plater-gui PRIVATE
  libplater
  Qt5::Widgets
  Qt5::OpenGL
  OpenGL::GLU
  OpenGL::GL
)

if(WIN32)
  # On Windows (including the MingW cross-compile) we need to link
  # some libraries manually as the transitive dependencies do not
  # include them. This function tries to find the QT library depending
  # on build configuration.
  function(target_link_qt_library target lib)
    if(MSVC)
      # With native MSVC build we can use real debug QT libraries
      set(DEBUG_SUFFIX d)
    else()
      set(DEBUG_SUFFIX)
    endif()
    if(MINGW)
      set(QT5_PREFIX qt5/)
    else()
      set(QT5_PREFIX)
    endif()
    find_library(${lib}_release ${lib}
      PATH_SUFFIXES
        ${QT5_PREFIX}plugins/platforms
        ${QT5_PREFIX}plugins/styles
    )
    find_library(${lib}_debug ${lib}${DEBUG_SUFFIX}
      PATH_SUFFIXES
        ${QT5_PREFIX}plugins/platforms
        ${QT5_PREFIX}plugins/styles
    )
    target_link_libraries(${target} PRIVATE
      $<IF:$<CONFIG:Debug>,${${lib}_debug},${${lib}_release}>
    )
  endfunction()

  target_compile_definitions(plater-gui PRIVATE QT_STATIC=1)
  # QT libraries
  target_link_qt_library(plater-gui qwindows)
  target_link_qt_library(plater-gui qwindowsvistastyle)
  target_link_qt_library(plater-gui Qt5EventDispatcherSupport)
  target_link_qt_library(plater-gui Qt5FontDatabaseSupport)
  target_link_qt_library(plater-gui Qt5ThemeSupport)
  target_link_qt_library(plater-gui Qt5VulkanSupport)
  target_link_qt_library(plater-gui Qt5WindowsUIAutomationSupport)
  # Win32 libraries
  target_link_libraries(plater-gui PRIVATE uxtheme)
  target_link_libraries(plater-gui PRIVATE wtsapi32)

  if(MINGW)
    # With MingW even more libraries need to be specified manually
    set(CMAKE_EXE_LINKER_FLAGS -static)
    target_link_options(plater-gui PRIVATE -static)
    target_link_libraries(plater-gui
      PRIVATE
       dwmapi
       freetype
       imm32
       png
       bz2
       z
       ssp
       winmm
    )
  endif()
endif()

install(TARGETS plater-gui RUNTIME DESTINATION bin)
if(MINGW)
  # install system DLL dependencies
  foreach(syslib
      iconv
      libgcc_s_seh-1
      libglib-2.0-0
      libharfbuzz-0
      libintl-8
      libpcre-1
      libpcre2-16-0
      libpng16-16
      libstdc++-6
      libssp-0
      libwinpthread-1
      Qt5Core
      Qt5Gui
      Qt5OpenGL
      Qt5Widgets
      zlib1
    )
    find_library(${syslib}_dll ${syslib})
    list(APPEND CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS ${${syslib}_dll})
  endforeach()
  include(InstallRequiredSystemLibraries)
endif()
