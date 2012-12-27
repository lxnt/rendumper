cmake_minimum_required(VERSION 2.8)
project(dfmodtests)

add_executable(test-alert alert.cpp)
target_link_libraries(test-alert ${GLUE_LIBS} ${CMAKE_THREAD_LIBS_INIT})
install(TARGETS test-alert RUNTIME DESTINATION bin)

add_executable(test-find find.cpp)
target_link_libraries(test-find ${GLUE_LIBS} ${CMAKE_THREAD_LIBS_INIT})
install(TARGETS test-find RUNTIME DESTINATION bin)

include_directories(../modules/common) # font8png.h
add_executable(test-life life.cpp)
target_link_libraries(test-life ${GLUE_LIBS} ${CMAKE_THREAD_LIBS_INIT})
install(TARGETS test-life RUNTIME DESTINATION bin)
