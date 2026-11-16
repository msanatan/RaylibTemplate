# Asset paths, desktop packing, and web preloading for the game target.
set(ASSETS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)
set(ASSET_IGNORED_FILES .DS_Store Thumbs.db desktop.ini)

file(GLOB_RECURSE ASSET_FILES CONFIGURE_DEPENDS RELATIVE ${ASSETS_SOURCE_DIR} ${ASSETS_SOURCE_DIR}/*)
foreach(ASSET_FILE IN LISTS ASSET_FILES)
  get_filename_component(ASSET_NAME ${ASSET_FILE} NAME)
  if(ASSET_NAME IN_LIST ASSET_IGNORED_FILES)
    list(REMOVE_ITEM ASSET_FILES ${ASSET_FILE})
  endif()
endforeach()
list(TRANSFORM ASSET_FILES PREPEND ${ASSETS_SOURCE_DIR}/ OUTPUT_VARIABLE ASSET_FILE_PATHS)

# Only rewritten when the set of assets changes, so depending on it catches deleted and renamed files
set(ASSETS_LIST_FILE ${CMAKE_BINARY_DIR}/assets.list)
list(JOIN ASSET_FILES "\n" ASSETS_LIST_CONTENT)
file(CONFIGURE OUTPUT ${ASSETS_LIST_FILE} CONTENT "${ASSETS_LIST_CONTENT}\n" @ONLY)

if(EMSCRIPTEN)
  set(WEB_SHELL_FILE ${CMAKE_BINARY_DIR}/shell.html)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html.in ${WEB_SHELL_FILE} @ONLY)
  target_link_options(${PROJECT_NAME} PRIVATE
    --shell-file ${WEB_SHELL_FILE}
    --preload-file ${ASSETS_SOURCE_DIR}@/assets
    $<$<CONFIG:Debug>:-sASSERTIONS=1>
  )
  foreach(ASSET_NAME IN LISTS ASSET_IGNORED_FILES)
    target_link_options(${PROJECT_NAME} PRIVATE "SHELL:--exclude-file */${ASSET_NAME}")
  endforeach()
  # Emscripten reads these at link time but doesn't report them as inputs, so edits wouldn't trigger a relink
  set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS "${WEB_SHELL_FILE};${ASSETS_LIST_FILE};${ASSET_FILE_PATHS}")
elseif(PACK_ASSETS)
  if(APPLE)
    set(ASSETS_PAK_DIR $<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources)
    set(ASSETS_PAK_PATH ../Resources/assets.pak)
  else()
    set(ASSETS_PAK_DIR $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    set(ASSETS_PAK_PATH assets.pak)
  endif()
  target_link_libraries(game PRIVATE miniz)
  target_compile_definitions(game PUBLIC "ASSETS_PAK_PATH=\"${ASSETS_PAK_PATH}\"")

  set(ASSETS_STAMP ${CMAKE_CURRENT_BINARY_DIR}/assets.stamp)
  add_custom_command(
    OUTPUT ${ASSETS_STAMP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ASSETS_PAK_DIR}
    COMMAND ${CMAKE_COMMAND} -E tar cf ${ASSETS_PAK_DIR}/assets.pak --format=zip --files-from=${ASSETS_LIST_FILE}
    COMMAND ${CMAKE_COMMAND} -E touch ${ASSETS_STAMP}
    WORKING_DIRECTORY ${ASSETS_SOURCE_DIR}
    DEPENDS ${ASSETS_LIST_FILE} ${ASSET_FILE_PATHS}
    COMMENT "Packing assets"
  )
  add_custom_target(pack_assets ALL DEPENDS ${ASSETS_STAMP})
else()
  target_compile_definitions(game PUBLIC "DEV_ASSETS_DIR=\"${ASSETS_SOURCE_DIR}/\"")
endif()
