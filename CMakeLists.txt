cmake_minimum_required(VERSION 3.10)
project(MetalSonic C)

# Path to a checkout of RSDKModding/Sonic-Mania-Decompilation (headers only are used)
set(DECOMP "" CACHE PATH "Sonic-Mania-Decompilation checkout")

add_library(MetalSonic SHARED MetalSonic.c)
set_target_properties(MetalSonic PROPERTIES C_STANDARD 11 POSITION_INDEPENDENT_CODE ON)

target_include_directories(MetalSonic PRIVATE ${DECOMP}/SonicMania ${DECOMP}/SonicMania/Objects)

# Same settings the decomp uses for a Mania Plus (v1.06) / RSDKv5U build
target_compile_definitions(MetalSonic PRIVATE
    RETRO_REVISION=3
    RETRO_USE_MOD_LOADER=1
    RETRO_MOD_LOADER_VER=2
    GAME_INCLUDE_EDITOR=0
    MANIA_PREPLUS=0
    MANIA_FIRST_RELEASE=0
    GAME_VERSION=6)
