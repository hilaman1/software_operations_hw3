project(HW3 C)

set(CMAKE_C_STANDARD 11)

add_executable(HW3 message_slot.c message_slot.h)
target_link_libraries(HW3 m)
