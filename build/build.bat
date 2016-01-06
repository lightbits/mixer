@echo off
pushd .\build

set CommonCompilerFlags=-Zi -nologo -Oi -Od -WX -W3 -wd4100 -fp:fast /MD
set CommonLinkerFlags=-subsystem:console -incremental:no -debug SDL2.lib SDL2main.lib

cl %CommonCompilerFlags% -D_CRT_SECURE_NO_WARNINGS -I../lib/sdl/include ../game.cpp /link %CommonLinkerFlags% -out:mixer.exe
REM mixer.exe
popd
