if(APPLE)
  add_library(SpellCircleAppNap STATIC
    AppNap.mm include/AppNap.h
  )

  target_link_libraries(SpellCircleAppNap
      PRIVATE
        ${FOUNDATION_FRAMEWORK}
  )

  target_include_directories(SpellCircleAppNap PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
  )
endif()
