cmake_minimum_required(VERSION 3.16)
file(READ "${CMAKE_CURRENT_LIST_DIR}/zion-product.json" ZION_PRODUCT_JSON)
string(JSON PROJECT_VER GET "${ZION_PRODUCT_JSON}" version)
string(JSON ZION_APP_PROJECT_NAME GET "${ZION_PRODUCT_JSON}" app_project_name)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(FoloToy-AI-Passport)

# Keep portable artifact paths while branding ESP-IDF's NUL-terminated 32-byte
# descriptor. The exact 32-byte display name is stored separately in MNAM.
idf_component_get_property(ZION_APP_FORMAT_DIR esp_app_format COMPONENT_DIR)
get_source_file_property(ZION_DESC_DEFINITIONS
    "${ZION_APP_FORMAT_DIR}/esp_app_desc.c" DIRECTORY "${ZION_APP_FORMAT_DIR}" COMPILE_DEFINITIONS)
list(FILTER ZION_DESC_DEFINITIONS EXCLUDE REGEX "^[ \t]*PROJECT_NAME=")
file(WRITE "${CMAKE_BINARY_DIR}/zion_app_name.h"
    "#define ZION_APP_PROJECT_NAME_UTF8 \"${ZION_APP_PROJECT_NAME}\"\n")
list(APPEND ZION_DESC_DEFINITIONS "PROJECT_NAME=ZION_APP_PROJECT_NAME_UTF8")
set_source_files_properties("${ZION_APP_FORMAT_DIR}/esp_app_desc.c"
    DIRECTORY "${ZION_APP_FORMAT_DIR}" PROPERTIES COMPILE_DEFINITIONS "${ZION_DESC_DEFINITIONS}")
set_property(SOURCE "${ZION_APP_FORMAT_DIR}/esp_app_desc.c" DIRECTORY "${ZION_APP_FORMAT_DIR}"
    APPEND PROPERTY COMPILE_OPTIONS "-include" "${CMAKE_BINARY_DIR}/zion_app_name.h")
