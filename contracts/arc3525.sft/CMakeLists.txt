add_contract(amax.stoken amax.stoken ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.stoken.cpp)

target_include_directories(amax.stoken
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.stoken
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.stoken.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.stoken.contracts.md @ONLY )

target_compile_options( amax.stoken PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
