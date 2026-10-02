cmake_minimum_required(VERSION 3.24)
project(JournalOwnershipProof LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
if(NOT COMPANION_SOURCE OR NOT COMPANION_BUILD)
    message(FATAL_ERROR "Pass the companion source and configured build directories")
endif()
find_package(Qt6 REQUIRED COMPONENTS Core Gui Network)
find_library(COMPANION_SYNC_LIBRARY NAMES OpenCloudLibSync
    PATHS "${COMPANION_BUILD}/bin" NO_DEFAULT_PATH REQUIRED)
add_executable(journal-probe main.cpp)
target_include_directories(journal-probe PRIVATE
    "${COMPANION_SOURCE}/src" "${COMPANION_SOURCE}/src/libsync"
    "${COMPANION_BUILD}/src" "${COMPANION_BUILD}/src/libsync")
target_link_libraries(journal-probe PRIVATE Qt6::Core Qt6::Gui Qt6::Network "${COMPANION_SYNC_LIBRARY}")
set_target_properties(journal-probe PROPERTIES BUILD_RPATH "${COMPANION_BUILD}/bin")
