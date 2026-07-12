add_library(bt_ml_signal_engine STATIC
  src/ml_signal_engine.cpp
)
target_include_directories(bt_ml_signal_engine PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_ml_signal_engine PUBLIC bt_linalg bt_stats)
target_compile_features(bt_ml_signal_engine PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_ml_signal_engine_tests
    tests/ml_signal_engine_unit_tests.cpp
    tests/compute_features_tests.cpp
    tests/predict_ridge_tests.cpp
  )
  target_link_libraries(bt_ml_signal_engine_tests PRIVATE bt_ml_signal_engine Catch2::Catch2WithMain)
  catch_discover_tests(bt_ml_signal_engine_tests)
endif()
