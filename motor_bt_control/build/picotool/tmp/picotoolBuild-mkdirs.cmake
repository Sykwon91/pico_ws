# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/kwon/pico_ws/motor_bt_control/build/_deps/picotool-src"
  "/home/kwon/pico_ws/motor_bt_control/build/_deps/picotool-build"
  "/home/kwon/pico_ws/motor_bt_control/build/_deps"
  "/home/kwon/pico_ws/motor_bt_control/build/picotool/tmp"
  "/home/kwon/pico_ws/motor_bt_control/build/picotool/src/picotoolBuild-stamp"
  "/home/kwon/pico_ws/motor_bt_control/build/picotool/src"
  "/home/kwon/pico_ws/motor_bt_control/build/picotool/src/picotoolBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/kwon/pico_ws/motor_bt_control/build/picotool/src/picotoolBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/kwon/pico_ws/motor_bt_control/build/picotool/src/picotoolBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()
