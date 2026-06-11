if(TARGET CinderNozzle)
    return()
endif()

set(NOZZLE_INSTALL OFF CACHE BOOL "" FORCE)
set(NOZZLE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NOZZLE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
if(NOT TARGET nozzle)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../deps/nozzle ${CMAKE_CURRENT_BINARY_DIR}/cinder-nozzle-nozzle-build)
endif()

add_library(CinderNozzle STATIC
    ${CMAKE_CURRENT_LIST_DIR}/../../src/cinder/nozzle/NozzleDiagnostics.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../src/cinder/nozzle/NozzleDiscovery.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../src/cinder/nozzle/NozzleReceiver.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../src/cinder/nozzle/NozzleSender.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../src/cinder/nozzle/PixelPattern.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../src/cinder/nozzle/Status.cpp
)
target_include_directories(CinderNozzle PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../../include
    ${CMAKE_CURRENT_LIST_DIR}/../../deps/nozzle/include
)
target_compile_features(CinderNozzle PUBLIC cxx_std_17)
target_link_libraries(CinderNozzle PUBLIC nozzle)
