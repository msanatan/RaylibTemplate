add_executable(Tuni src/main.cpp)
set_target_properties(Tuni PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_link_libraries(Tuni PRIVATE raylib)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
  target_compile_options(Tuni PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
endif()
