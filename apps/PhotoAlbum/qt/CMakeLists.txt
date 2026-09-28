cmake_minimum_required(VERSION 3.16)
project(photo_album LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Concurrent Network)

add_executable(photo-album
    main.cpp
    main_window.cpp main_window.h
    photo_view.cpp photo_view.h
    video_player.cpp video_player.h
    icm20608.cpp icm20608.h
    tank_game.cpp tank_game.h
    snake_game.cpp snake_game.h
    tetris_game.cpp tetris_game.h tetris_logic.cpp tetris_logic.h
    brick_game.cpp brick_game.h
    game2048.cpp game2048.h game2048_logic.cpp game2048_logic.h
    music_player.cpp music_player.h
    weather.cpp weather.h
    calculator.cpp calculator.h
    drawboard.cpp drawboard.h
    ap3216c.cpp ap3216c.h
    icon_factory.cpp icon_factory.h
    debug_page.cpp debug_page.h
    debug_monitor.cpp debug_monitor.h
    debug_overlay.cpp debug_overlay.h
)

target_link_libraries(photo-album PRIVATE
    Qt5::Widgets
    Qt5::Concurrent
    Qt5::Network
    avformat avcodec avutil swscale asound
)

# 单元测试（纯逻辑，主机运行）
option(PHOTOALBUM_BUILD_TESTS "Build unit tests" ON)
if(PHOTOALBUM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
