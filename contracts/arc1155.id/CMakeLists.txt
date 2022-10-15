add_contract(verso.itoken verso.itoken ${CMAKE_CURRENT_SOURCE_DIR}/src/verso.itoken.cpp)

target_include_directories(verso.itoken
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(verso.itoken
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/verso.itoken.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/verso.itoken.contracts.md @ONLY )

target_compile_options( verso.itoken PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
