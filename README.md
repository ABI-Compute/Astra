# Astra
## Intro
Welcome to Astra! An open source browser

## windows example
<img width="998" height="787" alt="image" src="https://github.com/user-attachments/assets/ecd45461-3b50-44e7-a715-884dc9a9f3b5" />

<img width="998" height="789" alt="image" src="https://github.com/user-attachments/assets/94f4cdec-61f8-4d82-bc16-3b8d56dedd7b" />


## Ubuntu example
<img width="809" height="630" alt="image" src="https://github.com/user-attachments/assets/b78b2dcf-1407-4151-b2ae-bf32e8cda598" />



## DETAILS
The Astra engine is still in **DEVELOPMENT** so it has some buggy and unstable features. Contact me in the issues section if you want to report an issue, and I'll try my best to fix it
The releases section has pre built binaries **OR** you can clone this repo and on Windows execute build.bat to build both 
To build it, you need SDL2, SDL_image and SDL_ttf installed on your system.

## Features
The Astra engine, while in very early stages of development has:
- HTML, CSS and JS parsing capabilities
- .ab IR generation
- HTML and CSS rendering using SDL2, SDL_image and SDL_ttf
- JS execution using a minimal JS engine
- A Dev console
- Refreshing
- A right click menu
  
## Build
There are 2 supported platforms: Windows and Ubuntu.
This will show you how to build in both

## Windows
1. install [SDL2](https://github.com/libsdl-org/SDL/releases), [SDL_ttf](https://github.com/libsdl-org/SDL_ttf/releases) and [SDL_image](https://github.com/libsdl-org/SDL_image/releases)
2. extract them to C:\SDL2\ (If you don't then change build.bat accordingly)
3. run build
4. run:
```cmd
conv YOURFILE.html
render YOURFILE.ab
```

## Ubuntu
1. install libraries:
```cmd
sudo apt update
sudo apt install build-essential g++ \
                 libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev
```

2. make script executable:
```cmd
chmod +x build.sh
```

3. run the script
```cmd
./build.sh
```

4. run:
```cmd
./conv YOURFILE.html
./render YOURFILE.ab
```
