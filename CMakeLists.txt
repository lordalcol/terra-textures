cmake_minimum_required(VERSION 3.20)
project(TerraTextures CXX C)

set(CMAKE_CXX_STANDARD 11)

# GLUT: freeglut, the maintained GLUT, downloaded and built as a static library.
include(FetchContent)
set(FREEGLUT_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(FREEGLUT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(FREEGLUT_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
FetchContent_Declare(freeglut
  GIT_REPOSITORY https://github.com/freeglut/freeglut.git
  GIT_TAG v3.8.0
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(freeglut)

find_package(OpenGL REQUIRED)

add_executable(TerraTextures TerraTextures/sfera.cpp TerraTextures/image.cpp TerraTextures/image.h)
target_include_directories(TerraTextures PRIVATE ${freeglut_SOURCE_DIR}/include)
target_compile_definitions(TerraTextures PRIVATE FREEGLUT_STATIC _CRT_SECURE_NO_WARNINGS)
target_link_libraries(TerraTextures PRIVATE freeglut_static OpenGL::GL OpenGL::GLU)

# The program loads its textures from the current folder: copy them next to
# it, and start it there from Visual Studio.
file(GLOB TEXTURES ${CMAKE_SOURCE_DIR}/TerraTextures/*.bmp)
add_custom_command(TARGET TerraTextures POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TEXTURES} $<TARGET_FILE_DIR:TerraTextures>)
set_target_properties(TerraTextures PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:TerraTextures>)
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT TerraTextures)
