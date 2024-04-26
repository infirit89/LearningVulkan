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

include "LearningVulkan"
