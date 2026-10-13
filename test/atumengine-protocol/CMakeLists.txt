cmake_minimum_required(VERSION 3.20)
project(AtumEngineProtocolConformance LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Test)
add_executable(testatumengineprotocol ../testatumengineprotocol.cpp ../../src/gui/atumengineprotocol.cpp)
target_include_directories(testatumengineprotocol PRIVATE ../../src)
target_compile_definitions(testatumengineprotocol PRIVATE SOURCEDIR="${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_compile_definitions(testatumengineprotocol PRIVATE
    QT_NO_CAST_FROM_ASCII QT_NO_CAST_FROM_BYTEARRAY QT_NO_CAST_TO_ASCII
    QT_NO_CONTEXTLESS_CONNECT QT_NO_KEYWORDS QT_NO_NARROWING_CONVERSIONS_IN_CONNECT
    QT_STRICT_ITERATORS QT_USE_QSTRINGBUILDER QT_NO_FOREACH QT_NO_URL_CAST_FROM_STRING
    QT_DISABLE_DEPRECATED_BEFORE=0x060200)
target_link_libraries(testatumengineprotocol PRIVATE Qt6::Core Qt6::Test)
enable_testing()
add_test(NAME testatumengineprotocol COMMAND testatumengineprotocol)
