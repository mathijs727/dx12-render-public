# Copied from:
# https://github.com/lefticus/cpp_starter_project

function(enable_sanitizers project_name)

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    option(ENABLE_COVERAGE "Enable coverage reporting for gcc/clang" FALSE)

    if(ENABLE_COVERAGE)
      target_compile_options(project_options INTERFACE --coverage -O0 -g)
      target_link_libraries(project_options INTERFACE --coverage)
    endif()

    set(SANITIZERS "")
    set(TRAP_SANITIZERS "")

    option(ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" FALSE)
    if(ENABLE_SANITIZER_ADDRESS)
      list(APPEND SANITIZERS "address")
    endif()

    option(ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" FALSE)
    if(ENABLE_SANITIZER_MEMORY)
      list(APPEND SANITIZERS "memory")
    endif()

    option(ENABLE_SANITIZER_UNDEFINED_BEHAVIOR "Enable undefined behavior sanitizer" FALSE)
    if(ENABLE_SANITIZER_UNDEFINED_BEHAVIOR)
      set(UB_SANITIZERS "undefined" "float-divide-by-zero" "unsigned-integer-overflow" "alignment" "implicit-conversion")
      #set(UB_SANITIZERS "undefined") # NOTE(Mathijs): unsigned-integer-overflow may trigger inside std::unordered_map (hash function).
      list(APPEND SANITIZERS ${UB_SANITIZERS})
      # With clang-cl, we use traps (illegal instruction error) instead of the nice printing functionality provided by the ubsan runtime library.
      # This is necessary because the UBSAN runtime uses static linking whereas all the dependencies are compiled with dynamic linking (vcpkg default).
      if (MSVC) # clang-cl
        #list(APPEND TRAP_SANITIZERS ${UB_SANITIZERS})
        # Suppress warnings (trap turns them into errors) in third-party code.
        #message("CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")
        set(UBSAN_BLACKLIST "${CMAKE_CURRENT_LIST_DIR}/ubsan_blacklist.txt")
		target_compile_options(${project_name} INTERFACE "-fsanitize-blacklist=${UBSAN_BLACKLIST}")
        # UBSan is only available with statically linked CRT (/MT). The application should match this to prevent linker errors.
        target_compile_options(${project_name} INTERFACE "/MT")
      endif()
    endif()

    option(ENABLE_SANITIZER_THREAD "Enable thread sanitizer" FALSE)
    if(ENABLE_SANITIZER_THREAD)
      list(APPEND SANITIZERS "thread")
    endif()

    list(JOIN SANITIZERS "," LIST_OF_SANITIZERS)
    list(JOIN TRAP_SANITIZERS "," LIST_OF_TRAP_SANITIZERS)

  endif()
  
  if(LIST_OF_SANITIZERS)
    if(NOT "${LIST_OF_SANITIZERS}" STREQUAL "")
      target_compile_options(${project_name}
                             INTERFACE -fsanitize=${LIST_OF_SANITIZERS})
      target_link_libraries(${project_name}
                            INTERFACE -fsanitize=${LIST_OF_SANITIZERS})
    endif()
  endif()

  if(LIST_OF_TRAP_SANITIZERS)
    if(NOT "${LIST_OF_TRAP_SANITIZERS}" STREQUAL "")
      message("LIST_OF_TRAP_SANITIZERS: ${LIST_OF_TRAP_SANITIZERS}")
      target_compile_options(${project_name}
                             INTERFACE -fsanitize-trap=${LIST_OF_TRAP_SANITIZERS})
      target_link_libraries(${project_name}
                            INTERFACE -fsanitize-trap=${LIST_OF_TRAP_SANITIZERS})
    endif()
  endif()

endfunction()


