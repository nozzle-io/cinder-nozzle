cmake_minimum_required(VERSION 3.20)
project(cinder_nozzle LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(cinder_nozzle STATIC
    src/cinder/nozzle/NozzleDiagnostics.cpp
    src/cinder/nozzle/NozzleDiscovery.cpp
    src/cinder/nozzle/NozzleReceiver.cpp
    src/cinder/nozzle/NozzleSender.cpp
    src/cinder/nozzle/PixelPattern.cpp
    src/cinder/nozzle/Status.cpp
)
target_include_directories(cinder_nozzle PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/nozzle/include
)

add_executable(cinder_nozzle_self_test tests/self_test.cpp)
target_link_libraries(cinder_nozzle_self_test PRIVATE cinder_nozzle)

enable_testing()
add_test(NAME cinder_nozzle_self_test COMMAND cinder_nozzle_self_test)
