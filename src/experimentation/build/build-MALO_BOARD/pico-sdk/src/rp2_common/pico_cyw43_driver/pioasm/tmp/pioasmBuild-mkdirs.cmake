# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/mnt/Data/Projects/malo_sao/MalO_SAO/lib/micropython/lib/pico-sdk/tools/pioasm"
  "/mnt/Data/Projects/malo_sao/MalO_SAO/src/build/build-MALO_BOARD/pioasm"
  "/mnt/Data/Projects/malo_sao/MalO_SAO/src/build/build-MALO_BOARD/pioasm-install"
  "/mnt/Data/Projects/malo_sao/MalO_SAO/src/build/build-MALO_BOARD/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/tmp"
  "/mnt/Data/Projects/malo_sao/MalO_SAO/src/build/build-MALO_BOARD/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
  "/mnt/Data/Projects/malo_sao/MalO_SAO/src/build/build-MALO_BOARD/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src"
  "/mnt/Data/Projects/malo_sao/MalO_SAO/src/build/build-MALO_BOARD/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/mnt/Data/Projects/malo_sao/MalO_SAO/src/build/build-MALO_BOARD/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/mnt/Data/Projects/malo_sao/MalO_SAO/src/build/build-MALO_BOARD/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()
