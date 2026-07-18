cmake_minimum_required(VERSION 3.28...3.30)

# ArZoom can be built:
# 1) inside the official obs-plugintemplate scaffold; or
# 2) directly against an installed libobs CMake package.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)
  project(${_name} VERSION ${_version} LANGUAGES C CXX)

  option(ENABLE_FRONTEND_API "Use obs-frontend-api" OFF)
  option(ENABLE_QT "Use Qt" OFF)

  include(compilerconfig)
  include(defaults)
  include(helpers)
else()
  project(arzoom VERSION 0.1.3 LANGUAGES C CXX)
  option(ENABLE_FRONTEND_API "Use obs-frontend-api" OFF)
  option(ENABLE_QT "Use Qt" OFF)
endif()

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/arzoom-filter.cpp
    src/arzoom-math.hpp
)

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)

if(MSVC)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W4 /permissive- /EHsc)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(COMMAND set_target_properties_plugin)
  set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${CMAKE_PROJECT_NAME})
else()
  set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "" OUTPUT_NAME ${CMAKE_PROJECT_NAME})
  install(
    TARGETS ${CMAKE_PROJECT_NAME}
    RUNTIME DESTINATION "obs-plugins/64bit"
    LIBRARY DESTINATION "obs-plugins/64bit"
  )
  install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/"
    DESTINATION "data/obs-plugins/${CMAKE_PROJECT_NAME}"
  )
endif()
