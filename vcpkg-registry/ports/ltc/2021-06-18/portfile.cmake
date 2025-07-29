vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO mathijs727/ltc_code
  REF 010e371c1d93a523310da663241799bd81768a0b
  SHA512 3795fdae0486ee247b6db036939bed78a39dca3df15256877aa1e2dfb763b38405c63f9085b36189f19ba0a109f6d2b970bcd06ed9949a79af3cb804ae24477b
  HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH ${SOURCE_PATH}
    OPTIONS -DLTC_BUILD_APP=OFF
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(CONFIG_PATH cmake/)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)