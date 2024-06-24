cmake_minimum_required(VERSION 3.5)
project(idl-codegen VERSION 0.1)
if (NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif(NOT DEFINED CMAKE_BUILD_TYPE)

option(PIRATE_LINK_STATIC "Link CDRGenerator statically" Off)
set(CMAKE_CXX_STANDARD 11)
set(PTHREAD_ADDED_LIB "pthread")
if (PIRATE_LINK_STATIC)
    set(PTHREAD_ADDED_LIB "-static -Wl,--whole-archive -lpthread -Wl,--no-whole-archive")
endif(PIRATE_LINK_STATIC)

add_subdirectory(ticpp-master)

if(NOT DEFINED ANTLR4_MODULE_PATH)
    message(FATAL_ERROR "ANTLR4_MODULE_PATH must be defined (eg. /usr/local/lib/cmake/antlr4)")
endif(NOT DEFINED ANTLR4_MODULE_PATH)

if(NOT DEFINED ANTLR4_JAR_LOCATION)
    message(FATAL_ERROR "ANTLR4_JAR_LOCATION must be defined")
endif(NOT DEFINED ANTLR4_JAR_LOCATION)

set(CMAKE_MODULE_PATH ${ANTLR4_MODULE_PATH})
set(IDL_CXX_FLAGS -Werror -Wall -Wextra -Wpedantic -Wno-attributes -Wno-unused-parameter)

include(antlr4-runtime-config)
include_directories(${ANTLR4_INCLUDE_DIR} ticpp-master)
link_directories(${ANTLR4_LIB_DIR})

include(antlr4-generator-config)
antlr4_generate(IDL ${CMAKE_CURRENT_SOURCE_DIR}/IDL.g4 BOTH -listener -visitor)
include_directories(${ANTLR4_INCLUDE_DIR_IDL})

SET(CDR_SOURCES
    "CDRBuildTypes.cpp"
    "CDRGenerator.cpp"
    "CDRTypes.cpp"
    "StructTypeSpec.cpp"
    "UnionTypeSpec.cpp"
    "ModuleDecl.cpp"
    "Annotations.cpp"
    "indent_facet.cpp"
    "DFDLGenerator.cpp"
)

add_executable(CDRGenerator CDRGeneratorMain.cpp ${CDR_SOURCES} ${ANTLR4_SRC_FILES_IDL})
target_compile_options(CDRGenerator PRIVATE ${IDL_CXX_FLAGS})
target_link_libraries(CDRGenerator antlr4_static ticpp ${PTHREAD_ADDED_LIB})
install(TARGETS CDRGenerator DESTINATION bin)

if(PIRATE_UNIT_TEST)
    find_package(GTest REQUIRED)
    include_directories(. ${GTEST_INCLUDE_DIR})

    file(GLOB IDL_TEST_SRC test/*.cpp)
    add_executable(idl_test ${IDL_TEST_SRC} ${CDR_SOURCES} ${ANTLR4_SRC_FILES_IDL})
    target_compile_options(idl_test PRIVATE ${IDL_CXX_FLAGS})
    target_link_libraries(idl_test ${GTEST_MAIN_LIBRARY} ${GTEST_LIBRARIES} antlr4_static ticpp ${PTHREAD_ADDED_LIB})

    file(GLOB IDL_REGRESSION_TEST_SRC test/output/c/*.c test/output/c/packed/*.c)
    add_library(idl-regression STATIC ${IDL_REGRESSION_TEST_SRC})
    target_compile_options(idl-regression PRIVATE ${IDL_CXX_FLAGS})

    file(GLOB IDL_REGRESSION_TEST_CXX_SRC test/output/cpp/*.cpp test/output/cpp/packed/*.cpp)
    add_library(idl-regression-cxx STATIC ${IDL_REGRESSION_TEST_CXX_SRC})
    target_compile_options(idl-regression-cxx PRIVATE ${IDL_CXX_FLAGS})

    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test/input DESTINATION .)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test/output DESTINATION .)

    add_custom_target(valgrind COMMAND valgrind
        --leak-check=full
        --error-exitcode=1
        ./idl_test
    )
endif(PIRATE_UNIT_TEST)
