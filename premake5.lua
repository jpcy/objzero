if _ACTION == nil then
	return
end

solution "objzero"
	configurations { "Release", "Debug" }
	if _OPTIONS["cc"] ~= nil then
		location(path.join("build", _ACTION) .. "_" .. _OPTIONS["cc"])
	else
		location(path.join("build", _ACTION))
	end
	platforms { "x86_64", "x86" }
	startproject "example"
	filter "platforms:x86"
		architecture "x86"
	filter "platforms:x86_64"
		architecture "x86_64"
	filter "configurations:Debug*"
		defines { "_DEBUG" }
		optimize "Debug"
		symbols "On"
	filter "configurations:Release"
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
    filter "system:linux"
        links { "m" }

project "tests"
	kind "ConsoleApp"
	language "C"
	cdialect "C99"
	files { "tests.c" }
    filter "system:linux"
        links { "m" }

