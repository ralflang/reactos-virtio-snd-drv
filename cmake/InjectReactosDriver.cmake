if(NOT DEFINED VIRTIO_SND_SOURCE_DIR)
    message(FATAL_ERROR "VIRTIO_SND_SOURCE_DIR must point to the driver repository.")
endif()

if(NOT COMMAND set_module_type OR
   NOT COMMAND add_importlibs OR
   NOT COMMAND add_driver_inf OR
   NOT COMMAND add_cd_file OR
   NOT TARGET virtio)
    return()
endif()

get_property(_virtiosnd_injected GLOBAL PROPERTY VIRTIO_SND_DRIVER_INJECTED)
if(_virtiosnd_injected)
    return()
endif()
set_property(GLOBAL PROPERTY VIRTIO_SND_DRIVER_INJECTED TRUE)

add_subdirectory(
    "${VIRTIO_SND_SOURCE_DIR}"
    "${CMAKE_BINARY_DIR}/virtiosnd")
