if ((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  target_sources_ifdef(CONFIG_ZMK_RGB_UNDERGLOW app PRIVATE src/behaviors/behavior_layer_ug.c)

  zephyr_include_directories(include)
endif()
