cmake_minimum_required(VERSION 3.16)

project(RPVerbReconstruction VERSION 0.1.0 DESCRIPTION "RPVerb Reconstruction CLAP plugin" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# The port reproduces the original's float operations in order: no fast-math, no FMA contraction,
# and byte-addressed state access needs relaxed aliasing.
set(EXACT_FLOAT_FLAGS -fno-fast-math -ffp-contract=off -fno-strict-aliasing)
set(INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/engine ${CMAKE_CURRENT_SOURCE_DIR}/clap-sdk/include)

add_library(rpverb_reconstruction MODULE src/plugin.cpp)
set_target_properties(rpverb_reconstruction PROPERTIES OUTPUT_NAME "RPVerbReconstruction")
target_include_directories(rpverb_reconstruction PRIVATE ${INCLUDES})
target_compile_options(rpverb_reconstruction PRIVATE -O2 -Wall -Wextra -fvisibility=hidden ${EXACT_FLOAT_FLAGS})

if(APPLE)
    set_target_properties(rpverb_reconstruction PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION clap
        MACOSX_BUNDLE_BUNDLE_NAME "RPVerb Reconstruction"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.rpverb-reconstruction.rpverb"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in
        MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0")
else()
    set_target_properties(rpverb_reconstruction PROPERTIES PREFIX "" SUFFIX ".clap")
endif()

if(BUILD_TESTING)
    enable_testing()
    add_executable(rpverb_golden_test tests/golden.cpp)
    target_include_directories(rpverb_golden_test PRIVATE ${INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_compile_options(rpverb_golden_test PRIVATE -O2 -Wall -Wextra ${EXACT_FLOAT_FLAGS})
    add_test(NAME golden COMMAND rpverb_golden_test)

    add_executable(rpverb_host_test tests/host_smoke.cpp)
    target_include_directories(rpverb_host_test PRIVATE ${INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_compile_options(rpverb_host_test PRIVATE -O2 -Wall -Wextra ${EXACT_FLOAT_FLAGS})
    target_link_libraries(rpverb_host_test PRIVATE ${CMAKE_DL_LIBS})
    if(APPLE)
        add_test(NAME host_smoke COMMAND rpverb_host_test $<TARGET_BUNDLE_DIR:rpverb_reconstruction>/Contents/MacOS/RPVerbReconstruction)
    else()
        add_test(NAME host_smoke COMMAND rpverb_host_test $<TARGET_FILE:rpverb_reconstruction>)
    endif()
endif()
