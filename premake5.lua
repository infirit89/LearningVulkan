require("premake-ecc/ecc")

workspace("LearningVulkan")

startproject("LearningVulkan")

configurations({
	"Debug",
	"Release",
})

flags({
	"MultiProcessorCompile",
})

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

include("vendor/optick")
include("vendor/GLFW")

VULKAN_SDK = os.getenv("VULKAN_SDK")

project("LearningVulkan")
kind("ConsoleApp")
language("C++")
cppdialect("C++20")
staticruntime("Off")

architecture("x86_64")

targetdir("%{prj.location}/bin/" .. outputdir)
objdir("%{prj.location}/bin-int/" .. outputdir)

files({
	"src/**.h",
	"src/**.cpp",

	"%{wks.location}/Vendor/stb/**.h",
	"%{wks.location}/Vendor/stb/**.cpp",
})

includedirs({
	"src",
	"%{wks.location}/Vendor/GLFW/include",
	"%{wks.location}/Vendor/glm",
	"%{VULKAN_SDK}/Include",
	"%{wks.location}/Vendor/Optick/src/",
	"%{wks.location}/Vendor/stb",
})

links({
	"GLFW",
	"%{VULKAN_SDK}/Lib/vulkan-1.lib",
	"OptickCore",
})

defines({
	"_CRT_SECURE_NO_WARNINGS",
})

filter("system:windows")
systemversion("latest")

postbuildcommands({
	"{COPY} %{wks.location}Vendor/Optick/bin/%{outputdir}/OptickCore.dll %{prj.location}/bin/%{outputdir}",
})

filter("configurations:Debug")
defines("DEBUG")
runtime("Debug")
symbols("on")

filter("configurations:Release")
defines("RELEASE")
runtime("Release")
optimize("on")
