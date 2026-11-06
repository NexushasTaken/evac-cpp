install(
    TARGETS evac-cpp_exe
    RUNTIME COMPONENT evac-cpp_Runtime
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
