if (NOT TARGET pico_audio_i2s_sender)
    add_library(pico_audio_i2s_sender INTERFACE)

    pico_generate_pio_header(pico_audio_i2s_sender ${CMAKE_CURRENT_LIST_DIR}/pico_i2s.pio)
    pico_generate_pio_header(pico_audio_i2s_sender ${CMAKE_CURRENT_LIST_DIR}/pico_i2s_24bit.pio)

    target_sources(pico_audio_i2s_sender INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/pico_i2s.cpp
    )

    target_include_directories(pico_audio_i2s_sender INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
    target_link_libraries(pico_audio_i2s_sender INTERFACE hardware_dma hardware_pio hardware_irq)
endif()
