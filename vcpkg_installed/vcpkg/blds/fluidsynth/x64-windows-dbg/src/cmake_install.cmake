# Install script for directory: C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/src/v2.6.2-3edec6f02d.clean/src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/SoundModule/vcpkg_installed/vcpkg/pkgs/fluidsynth_x64-windows/debug")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "OFF")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fluidsynth_program" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/x64-windows-dbg/src/fluidsynth.exe")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fluidsynth_development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/x64-windows-dbg/src/libfluidsynth-3.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fluidsynth_runtime" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/x64-windows-dbg/src/libfluidsynth-3.dll")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fluidsynth_development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/fluidsynth" TYPE FILE FILES
    "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/src/v2.6.2-3edec6f02d.clean/include/fluidsynth/audio.h"
    "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/src/v2.6.2-3edec6f02d.clean/include/fluidsynth/event.h"
    "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/src/v2.6.2-3edec6f02d.clean/include/fluidsynth/gen.h"
    "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/src/v2.6.2-3edec6f02d.clean/include/fluidsynth/ladspa.h"
    "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/src/v2.6.2-3edec6f02d.clean/include/fluidsynth/log.h"
    "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/src/v2.6.2-3edec6f02d.clean/include/fluidsynth/midi.h"
    "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/src/v2.6.2-3edec6f02d.clean/include/fluidsynth/misc.h"
    "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/src/v2.6.2-3edec6f02d.clean/include/fluidsynth/mod.h"
    "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/src/v2.6.2-3edec6f02d.clean/include/fluidsynth/seq.h"
    "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/src/v2.6.2-3edec6f02d.clean/include/fluidsynth/seqbind.h"
    "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/src/v2.6.2-3edec6f02d.clean/include/fluidsynth/settings.h"
    "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/src/v2.6.2-3edec6f02d.clean/include/fluidsynth/sfont.h"
    "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/src/v2.6.2-3edec6f02d.clean/include/fluidsynth/shell.h"
    "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/src/v2.6.2-3edec6f02d.clean/include/fluidsynth/synth.h"
    "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/src/v2.6.2-3edec6f02d.clean/include/fluidsynth/types.h"
    "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/src/v2.6.2-3edec6f02d.clean/include/fluidsynth/voice.h"
    "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/x64-windows-dbg/include/fluidsynth/version.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fluidsynth_development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/x64-windows-dbg/include/fluidsynth.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fluidsynth_development" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/fluidsynth/FluidSynth-shared-targets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/fluidsynth/FluidSynth-shared-targets.cmake"
         "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/x64-windows-dbg/src/CMakeFiles/Export/6a6ea4714c6c3916014c1d187313a071/FluidSynth-shared-targets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/fluidsynth/FluidSynth-shared-targets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/fluidsynth/FluidSynth-shared-targets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/fluidsynth" TYPE FILE FILES "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/x64-windows-dbg/src/CMakeFiles/Export/6a6ea4714c6c3916014c1d187313a071/FluidSynth-shared-targets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/fluidsynth" TYPE FILE FILES "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/x64-windows-dbg/src/CMakeFiles/Export/6a6ea4714c6c3916014c1d187313a071/FluidSynth-shared-targets-debug.cmake")
  endif()
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/SoundModule/vcpkg_installed/vcpkg/blds/fluidsynth/x64-windows-dbg/src/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
