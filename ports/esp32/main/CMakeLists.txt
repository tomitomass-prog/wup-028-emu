set(GCCA_CORE ${CMAKE_CURRENT_LIST_DIR}/../../../src)
idf_component_register(
    SRCS "main.c" "bridge.c" "${GCCA_CORE}/usb_descriptors.c"
    INCLUDE_DIRS "." "${GCCA_CORE}"
    REQUIRES esp_tinyusb driver
)
