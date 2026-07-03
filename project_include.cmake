# This file is automatically included by ESP-IDF before processing components.
# It applies a patch to the registry-managed esp_matter component so that its
# compile options are gated by COMPILE_LANGUAGE:C,CXX (Swift sources reject the
# -Wno-error=... and -std=gnu++17 flags otherwise).
#
# The patch is inert when the managed component is absent.  When esp_matter
# is fetched into managed_components/ by the IDF Component Manager, the patch
# is applied once and a sentinel file prevents re-application.

set(ESP_MATTER_PATCH "${CMAKE_CURRENT_LIST_DIR}/esp_matter.patch")
set(ESP_MATTER_DIR "${CMAKE_SOURCE_DIR}/managed_components/espressif__esp_matter")
set(ESP_MATTER_PATCH_APPLIED "${ESP_MATTER_DIR}/PATCH_APPLIED")

if(EXISTS "${ESP_MATTER_PATCH}" AND EXISTS "${ESP_MATTER_DIR}")
    if(NOT EXISTS "${ESP_MATTER_PATCH_APPLIED}")
        message(STATUS "Applying patch to esp_matter component...")
        execute_process(
            COMMAND patch -p1 -i "${ESP_MATTER_PATCH}" --batch --force
            WORKING_DIRECTORY "${ESP_MATTER_DIR}"
            RESULT_VARIABLE PATCH_RESULT
            OUTPUT_VARIABLE PATCH_OUTPUT
            ERROR_VARIABLE PATCH_ERROR
        )
        if(PATCH_RESULT EQUAL 0)
            message(STATUS "Patch applied successfully")
            file(WRITE "${ESP_MATTER_PATCH_APPLIED}" "")
        else()
            message(STATUS "Patch output: ${PATCH_OUTPUT}")
            message(STATUS "Patch error: ${PATCH_ERROR}")
            message(WARNING "Failed to apply patch (result code: ${PATCH_RESULT})")
        endif()
    else()
        message(STATUS "ESP Matter patch already applied")
    endif()
else()
    message(STATUS "esp_matter managed component not found; skipping SwiftMatter patch")
endif()
