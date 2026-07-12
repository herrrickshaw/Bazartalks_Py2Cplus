# Overlay port for databento/databento-cpp -- no upstream vcpkg registry port
# exists as of this checkout (verified against the pinned vcpkg commit, which
# matched upstream HEAD at write time). Forcing the three DATABENTO_USE_EXTERNAL_*
# flags ON makes the upstream CMakeLists consume vcpkg's own cpp-httplib/date/
# nlohmann-json instead of FetchContent-ing its own copies at build time --
# this avoids a diamond dependency with this repo's own vcpkg-provided `date`
# and `nlohmann-json`, and keeps the whole build inside vcpkg's manifest-mode
# cache (no network fetch during the actual build step). See
# docs/MIGRATION_PLAN.md's Databento integration section for why an overlay
# port was chosen over the upstream README's own FetchContent suggestion.

vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO databento/databento-cpp
  REF "v${VERSION}"
  SHA512 39f1f43758de1e21c533b6e92546dc924650769e968b2b5a21c655e3feca9c51ec82698688a67ec17dadc6ba61b5181a25fd585cdef22ffe50ff80360f7c6779
  HEAD_REF main
)

vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
  OPTIONS
    -DDATABENTO_USE_EXTERNAL_JSON=ON
    -DDATABENTO_USE_EXTERNAL_HTTPLIB=ON
    -DDATABENTO_USE_EXTERNAL_DATE=ON
    -DDATABENTO_ENABLE_UNIT_TESTING=OFF
    -DDATABENTO_ENABLE_EXAMPLES=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/databento" PACKAGE_NAME databento)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
