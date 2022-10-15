add_contract(verso.ntoken verso.ntoken ${CMAKE_CURRENT_SOURCE_DIR}/src/verso.ntoken.cpp)

target_include_directories(verso.ntoken
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(verso.ntoken
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/verso.ntoken.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/verso.ntoken.contracts.md @ONLY )

target_compile_options( verso.ntoken PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
