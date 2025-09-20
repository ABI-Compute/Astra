#!/bin/bash

g++ render.cpp -o render \
    -I/usr/include/SDL2 \
    -lSDL2 -lSDL2_image -lSDL2_ttf

g++ conv.cpp -o conv

echo "Build complete!"
