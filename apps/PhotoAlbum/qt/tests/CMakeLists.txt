find_package(Qt5 REQUIRED COMPONENTS Test Core)

add_executable(tests
    tst_games.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../game2048_logic.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tetris_logic.cpp
)

target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(tests PRIVATE Qt5::Test Qt5::Core)

add_test(NAME games COMMAND tests)
