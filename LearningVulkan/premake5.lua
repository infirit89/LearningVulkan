VULKAN_SDK = os.getenv("VULKAN_SDK")

project "LearningVulkan"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    architecture "x86_64"

    targetdir ("%{prj.location}/bin/" .. outputdir)
    objdir ("%{prj.location}/bin-int/" .. outputdir)

    files 
    {
        "src/**.h",
        "src/**.cpp",
    }

    includedirs
    {
        "src",
        "%{wks.location}/Vendor/GLFW/include",
        "%{wks.location}/Vendor/glm",
        "%{VULKAN_SDK}/Include"
    }

    links
    {
        "GLFW",
        "%{VULKAN_SDK}/Lib/vulkan-1.lib"
    }
    
    defines 
    {
        "_CRT_SECURE_NO_WARNINGS"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        defines "DEBUG"
        runtime "Debug"
        symbols "on"


    filter "configurations:Release"
        defines "RELEASE"
        runtime "Release"
        optimize "on"
