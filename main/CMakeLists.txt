idf_component_register(
    SRCS main.c controller.c serial.c bridge.c cycle.c thread_entry.c follow.c
         diagnostics.c critical.c task_affinity.c network.c storage.c tmc.c spindle.c
         "../dependencies/trinamic/tmc5160.c" "../dependencies/trinamic/common.c"
    LDFRAGMENTS "motion.lf"
    INCLUDE_DIRS "."
    REQUIRES grblhal_p4 esp_wifi_remote esp_hosted app_update esp_wifi esp_event
             esp_netif mbedtls nvs_flash esp_driver_spi lathe_ui)
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    LATHE_BENCH_ONLY=$<BOOL:${LATHE_BENCH_ONLY}>
    LATHE_OTA_REQUIRE_PAIRING=$<BOOL:${LATHE_OTA_REQUIRE_PAIRING}>)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-missing-field-initializers -Wno-implicit-fallthrough)
target_link_options(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=xTaskCreatePinnedToCore" "-Wl,--wrap=xTaskCreateStaticPinnedToCore")
