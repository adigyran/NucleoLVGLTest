cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(h743_led_lab)
target_sources(app PRIVATE
    src/main.c
    src/app_main.cpp
    src/rtc_service.cpp
    src/rtc_shell.cpp
    src/lvgl_demo.cpp
    src/fpu_demo.cpp
    src/msgq_demo.cpp
    src/cpp_examples.cpp
    src/top_stats.cpp
    src/monitor/top_collector.cpp
    src/monitor/top_renderer.cpp
)

# Подключаем сгенерированные SquareLine Studio 1.6.x файлы (экспортируются в ui/).
# filelist.txt создаётся автоматически при каждом экспорте из редактора.
set(SLS_FILELIST ${CMAKE_CURRENT_SOURCE_DIR}/ui/filelist.txt)
if(EXISTS ${SLS_FILELIST})
    file(STRINGS ${SLS_FILELIST} SLS_SOURCES)
    list(TRANSFORM SLS_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/ui/)
    target_sources(app PRIVATE ${SLS_SOURCES})
    target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ui)
endif()
 
# Build a minimal loadable extension sample as build/hello_world_ext.llext.
add_llext_target(hello_world_ext
    OUTPUT  ${PROJECT_BINARY_DIR}/hello_world_ext.llext
    SOURCES ${PROJECT_SOURCE_DIR}/llext/hello_world_ext.c
)
