require "export-compile-commands"
require "premake-ecc/ecc"

workspace "LearningVulkan"

    startproject "LearningVulkan"
    
    configurations 
    {
        "Debug",
        "Release"
    }

    flags 
    {
        "MultiProcessorCompile"
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

include "Vendor/optick"
include "Vendor/GLFW"

include "LearningVulkan"

