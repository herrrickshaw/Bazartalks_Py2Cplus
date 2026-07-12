add_library(bt_universe STATIC
  src/survivorship.cpp
  src/pit_panel.cpp
)
target_include_directories(bt_universe PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_universe PUBLIC date::date)
target_compile_features(bt_universe PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_universe_tests
    tests/survivorship_tests.cpp
    tests/pit_panel_tests.cpp
  )
  target_link_libraries(bt_universe_tests PRIVATE bt_universe Catch2::Catch2WithMain)
  catch_discover_tests(bt_universe_tests)
endif()
