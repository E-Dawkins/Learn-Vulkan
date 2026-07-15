@echo off

%VULKAN_SDK%/Bin/glslc.exe shader.vert -o shader.vert.spv
if errorLevel 1 (
	echo shader.vert compilation failed!
	echo(
	pause
	exit /b 1
)

%VULKAN_SDK%/Bin/glslc.exe shader.frag -o shader.frag.spv
if errorLevel 1 (
	echo shader.frag compilation failed!
	echo(
	pause
	exit /b 1
)

echo shader.vert + shader.frag compilation successful.

%VULKAN_SDK%/Bin/glslc.exe skybox.vert -o skybox.vert.spv
if errorLevel 1 (
	echo skybox.vert compilation failed!
	echo(
	pause
	exit /b 1
)

%VULKAN_SDK%/Bin/glslc.exe skybox.frag -o skybox.frag.spv
if errorLevel 1 (
	echo skybox.frag compilation failed!
	echo(
	pause
	exit /b 1
)

echo skybox.vert + skybox.frag compilation successful.

echo(
pause