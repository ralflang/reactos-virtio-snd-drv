option(VIRTIO_SND_TEST_XRUN_INJECTION
    "Inject one synthetic XRUN per playback stream for recovery testing"
    OFF)

function(_configure_virtiosnd)
    if(NOT COMMAND set_module_type OR
       NOT COMMAND add_importlibs OR
       NOT COMMAND add_driver_inf OR
       NOT COMMAND add_cd_file OR
       NOT TARGET virtio)
        message(FATAL_ERROR
            "virtiosnd requires ReactOS driver macros and the virtio target. "
            "Configure it through scripts/build-reactos.ps1.")
    endif()

    add_library(virtiosnd MODULE
        "${VIRTIO_SND_SOURCE_DIR}/src/adapter.c"
        "${VIRTIO_SND_SOURCE_DIR}/src/audio_controls.c"
        "${VIRTIO_SND_SOURCE_DIR}/src/driver.c"
        "${VIRTIO_SND_SOURCE_DIR}/src/transport.c"
        "${VIRTIO_SND_SOURCE_DIR}/src/topology.cpp"
        "${VIRTIO_SND_SOURCE_DIR}/src/wavecyclic.cpp"
        "${VIRTIO_SND_SOURCE_DIR}/src/virtio_debug.c")

    target_include_directories(virtiosnd PRIVATE
        "${VIRTIO_SND_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/sdk/lib/drivers/virtio")

    target_link_libraries(virtiosnd virtio stdunk libcntpr uuid)
    if(VIRTIO_SND_TEST_XRUN_INJECTION)
        target_compile_definitions(virtiosnd PRIVATE
            VIRTIO_SND_TEST_XRUN_INJECTION=1)
        message(STATUS "Building virtiosnd with synthetic XRUN injection enabled")
    endif()

    set_module_type(virtiosnd wdmdriver)
    add_importlibs(virtiosnd portcls hal ntoskrnl)
    add_cd_file(TARGET virtiosnd DESTINATION reactos/system32/drivers FOR all)
    add_driver_inf(virtiosnd "${VIRTIO_SND_SOURCE_DIR}/virtiosnd.inf")
endfunction()

cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _configure_virtiosnd)
