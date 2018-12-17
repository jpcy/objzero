solution "objzero"
	configurations { "Release", "Debug" }
	location(path.join("build", _ACTION))
	if os.is64bit() and not os.istarget("windows") then
		platforms { "x86_64", "x86" }
	else
		platforms { "x86", "x86_64" }
	end
	startproject "example"
	filter "platforms:x86"
		architecture "x86"
	filter "platforms:x86_64"
		architecture "x86_64"
	filter "configurations:Debug*"
		defines { "_DEBUG" }
		optimize "Debug"
		symbols "On"
	filter { "configurations:Release" }
		defines "NDEBUG"
		optimize "Full"
		
project "objzero"
	kind "StaticLib"
	language "C"
	cdialect "C99"
	warnings "Extra"
	files { "objzero.c", "objzero.h" }

project "example"
	kind "ConsoleApp"
	language "C"
	cdialect "C99"
	warnings "Extra"
	files { "example.c" }
	links { "objzero" }

project "tests"
	kind "ConsoleApp"
	language "C"
	cdialect "C99"
	files { "tests.c" }
