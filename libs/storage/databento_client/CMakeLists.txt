# databento-cpp's own installed databentoConfig.cmake calls find_dependency()
# for date/httplib/zstd/nlohmann_json/Threads but NOT OpenSSL, even though
# databento::databento's link interface includes OpenSSL::Crypto/OpenSSL::SSL
# (it links OpenSSL unconditionally in its own top-level CMakeLists, but that
# find_package() call isn't propagated into the installed config template --
# an upstream gap, not a mistake in this port). Finding it here first, before
# find_package(databento ...), means the OpenSSL::* targets already exist by
# the time databentoTargets.cmake references them.
find_package(OpenSSL REQUIRED)
find_package(databento CONFIG REQUIRED)

add_library(bt_databento_client STATIC
  src/databento_client.cpp
)
target_include_directories(bt_databento_client PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_databento_client PUBLIC
  bt_sqlite_client
  databento::databento
)
target_compile_features(bt_databento_client PUBLIC cxx_std_20)

# cpp-httplib's macOS TLS backend (vendored inside libdatabento.a) calls into
# Security.framework for system cert-store access; databento's own installed
# usage-requirements list CFNetwork/CoreFoundation but not Security, which
# otherwise fails at link time with undefined SecCertificateCopyData/
# SecTrustSettingsCopyCertificates symbols.
if(APPLE)
  target_link_libraries(bt_databento_client PUBLIC "-framework Security")
endif()

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_databento_client_tests tests/databento_client_tests.cpp)
  target_link_libraries(bt_databento_client_tests PRIVATE bt_databento_client Catch2::Catch2WithMain)
  catch_discover_tests(bt_databento_client_tests)

  # Separate target, deliberately -- a normal unit-test run never links
  # network code. Skips/no-ops at runtime unless DATABENTO_API_KEY is set;
  # when it does run, it costs a small real amount (see the test file's
  # header comment for the approximate figure).
  add_executable(bt_databento_client_integration_tests tests/databento_client_integration_tests.cpp)
  target_link_libraries(bt_databento_client_integration_tests PRIVATE bt_databento_client Catch2::Catch2WithMain)
  catch_discover_tests(bt_databento_client_integration_tests)
endif()
