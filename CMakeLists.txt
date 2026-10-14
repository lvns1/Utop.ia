cmake_minimum_required(VERSION 3.15)
project(UtopIA C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(raylib QUIET)
if (NOT raylib_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.0
    )
    FetchContent_MakeAvailable(raylib)
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/utils
    ${CMAKE_SOURCE_DIR}/entities
    ${CMAKE_SOURCE_DIR}/systems
    ${CMAKE_SOURCE_DIR}/data
    ${CMAKE_SOURCE_DIR}/scenes
)

set(SOURCES
    main.c
    core/game.c
    core/transition.c
    utils/text.c
    utils/effects.c
    entities/player.c
    entities/luci.c
    entities/monitor.c
    entities/projectile.c
    systems/collision.c
    systems/dialogue.c
    systems/quiz.c
    systems/battle.c
    data/dialogues.c
    data/quizzes.c
    scenes/menu.c
    scenes/intro.c
    scenes/sala.c
    scenes/corredor.c
)

add_executable(utopia ${SOURCES})
target_link_libraries(utopia raylib)

if (UNIX AND NOT APPLE)
    target_link_libraries(utopia m pthread dl rt X11)
endif()
