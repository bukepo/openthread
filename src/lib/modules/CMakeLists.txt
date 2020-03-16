add_library(openthread-radio-modules
    radio.hpp
    radio.cpp
)

target_link_libraries(openthread-radio-modules PRIVATE ot-config)
