#!/bin/bash
./script/cmake-build simulation \
    -DBUILD_TESTING=OFF \
    -DOT_BUILD_GTEST=OFF \
    -DOT_CHANNEL_MANAGER=OFF \
    -DOT_CHANNEL_MONITOR=OFF \
    -DOT_LOG_MAX_SIZE=1024 \
    -DOT_LOG_OUTPUT=PLATFORM_DEFINED \
    "$@"
