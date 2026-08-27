cmake_minimum_required(VERSION 3.5)
project(wiliwili_lite_f1c200s CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL REQUIRED sdl)
pkg_check_modules(SDL_TTF REQUIRED SDL_ttf)
pkg_check_modules(CURL REQUIRED libcurl)

add_executable(wiliwili-lite src/main.cpp)
target_include_directories(wiliwili-lite PRIVATE
    ${SDL_INCLUDE_DIRS} ${SDL_TTF_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS})
target_link_libraries(wiliwili-lite
    ${SDL_LIBRARIES} ${SDL_TTF_LIBRARIES} ${CURL_LIBRARIES})
target_compile_options(wiliwili-lite PRIVATE -Wall -Wextra -Werror)
