# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/mnt/Data/Projects/malo_sao/MalO_SAO/src/build/build-MALO_BOARD/_deps/picotool-src"
  "/mnt/Data/Projects/malo_sao/MalO_SAO/src/build/build-MALO_BOARD/_deps/picotool-build"
  "/mnt/Data/Projects/malo_sao/MalO_SAO/src/build/build-MALO_BOARD/_deps"
  "/mnt/Data/Projects/malo_sao/MalO_SAO/src/build/build-MALO_BOARD/pico-sdk/src/rp2350/boot_stage2/picotool/tmp"
  "/mnt/Data/Projects/malo_sao/MalO_SAO/src/build/build-MALO_BOARD/pico-sdk/src/rp2350/boot_stage2/picotool/src/picotoolBuild-stamp"
  "/mnt/Data/Projects/malo_sao/MalO_SAO/src/build/build-MALO_BOARD/pico-sdk/src/rp2350/boot_stage2/picotool/src"
  "/mnt/Data/Projects/malo_sao/MalO_SAO/src/build/build-MALO_BOARD/pico-sdk/src/rp2350/boot_stage2/picotool/src/picotoolBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/mnt/Data/Projects/malo_sao/MalO_SAO/src/build/build-MALO_BOARD/pico-sdk/src/rp2350/boot_stage2/picotool/src/picotoolBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/mnt/Data/Projects/malo_sao/MalO_SAO/src/build/build-MALO_BOARD/pico-sdk/src/rp2350/boot_stage2/picotool/src/picotoolBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()
