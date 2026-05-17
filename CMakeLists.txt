cmake_minimum_required(VERSION 3.20)
project(FUK_project C)

# --- Настройки стандартов и компилятора ---
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)


# Включаем полезные предупреждения (Warnings) для отлова ошибок
if(MSVC)
add_compile_options(/W4)
else()
add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# "MyExecutable" is the name of the program.
# "main.cpp" is your source file.
add_executable(main
        main.c
        init.c
        add_remove.c
        z_compressor.c
        commit.c
        auxiliary_functions.c)


target_link_libraries(main PRIVATE OpenSSL::Crypto ZLIB::ZLIB)