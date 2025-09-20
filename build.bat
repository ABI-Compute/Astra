g++ render.cpp -o render.exe ^
   -IC:/SDL2/SDL2-2.32.8/x86_64-w64-mingw32/include/SDL2 ^
   -IC:/SDL2/SDL2_image-2.6.0/x86_64-w64-mingw32/include/SDL2 ^
   -IC:/SDL2/SDL2_ttf-2.22.0/x86_64-w64-mingw32/include/SDL2 ^
   -LC:/SDL2/SDL2-2.32.8/x86_64-w64-mingw32/lib ^
   -LC:/SDL2/SDL2_image-2.6.0/x86_64-w64-mingw32/lib ^
   -LC:/SDL2/SDL2_ttf-2.22.0/x86_64-w64-mingw32/lib ^
   -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf
   
g++ conv.cpp -o conv.exe

echo Build complete!