@echo off

call _compile_single.bat "shader.frag"
call _compile_single.bat "shader.vert"
call _compile_single.bat "skybox.frag"
call _compile_single.bat "skybox.vert"

echo All shaders compiled successfully!
pause