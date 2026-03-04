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

include("vendor/GLFW")

VULKAN_SDK = os.getenv("VULKAN_SDK")
print(VULKAN_SDK .. "/lib/libvulkan.1.4.313.dylib")

project("LearningVulkan")
kind("ConsoleApp")
language("C++")
cppdialect("C++20")
staticruntime("Off")

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
	"%{wks.location}/Vendor/stb",
})

links({
	"GLFW",
})

filter("system:windows")
architecture("x64")
systemversion("latest")
links({
	"%{VULKAN_SDK}/Lib/vulkan-1.lib",
})
defines({
	"_CRT_SECURE_NO_WARNINGS",
})

filter("system:macosx")
architecture("ARM64")

libdirs({ "/usr/local/lib", "%{VULKAN_SDK}/lib" })
links({
	"CoreFoundation.framework", -- no path needed for system frameworks
	"Cocoa.framework",
	"IOKit.framework",
	"QuartzCore.framework",
	"vulkan",
})

filter("configurations:Debug")
defines("DEBUG")
runtime("Debug")
symbols("on")

filter("configurations:Release")
defines("RELEASE")
runtime("Release")
optimize("on")
