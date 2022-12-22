project(HW3 C)

set(CMAKE_C_STANDARD 11)

add_executable(HW3 message_reader.c)
target_link_libraries(HW3 m)
