# THE LIST OF PLUGINS AND THE CORRESPONDING SOURCE FILES
# ======================================================
set(LLVM_EXECISE_PLUGINS
   
    # OpcodeCounter
    # InjectFuncCall
    # StaticCallCounter
    # DynamicCallCounter
    # MBASub
    # MBAAdd
    # RIV
    # DuplicateBB
    # MergeBB
    # FindFCmpEq
    # ConvertFCmpEq
    # MemoryTrace
    # FunctionInfo
    # Transform
    # StrengthReductionPass
    ModuleMaker
    )


# set(OpcodeCounter_SOURCES
#   OpcodeCounter.cpp)
# set(InjectFuncCall_SOURCES
#   InjectFuncCall.cpp)  
# set(StaticCallCounter_SOURCES
#   StaticCallCounter.cpp) 
# set(DynamicCallCounter_SOURCES
#   DynamicCallCounter.cpp)     
# set(MBASub_SOURCES
#   MBASub.cpp) 
# set(MBAAdd_SOURCES
#   MBAAdd.cpp)  
# set(RIV_SOURCES
#   RIV.cpp) 
# set(DuplicateBB_SOURCES
#   DuplicateBB.cpp) 
# set(MergeBB_SOURCES
#   MergeBB.cpp)  
# set(FindFCmpEq_SOURCES
#   FindFCmpEq.cpp)   
# set(ConvertFCmpEq_SOURCES
#   ConvertFCmpEq.cpp)
# set(MemoryTrace_SOURCES
#   MemoryTrace.cpp)  
# set(FunctionInfo_SOURCES
#   FunctionInfo.cpp) 
# set(Transform_SOURCES
#   Transform.cpp)   
# set(AlgebraicIdentityPass_SOURCES
#     AlgebraicIdentityPass.cpp)
# set(StrengthReductionPass_SOURCES
#       StrengthReductionPass.cpp)
set(ModuleMaker_SOURCES
ModuleMaker.cpp)

# CONFIGURE THE PLUGIN LIBRARIES
# ==============================
foreach( plugin ${LLVM_EXECISE_PLUGINS} )
    # Create a library corresponding to 'plugin'
    add_library(
      ${plugin}
      SHARED
      ${${plugin}_SOURCES}
      )

    # Configure include directories for 'plugin'
    target_include_directories(
      ${plugin}
      PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/../include"
    )

    # On Darwin (unlike on Linux), undefined symbols in shared objects are not
    # allowed at the end of the link-edit. The plugins defined here:
    #  - _are_ shared objects
    #  - reference symbols from LLVM shared libraries, i.e. symbols which are
    #    undefined until those shared objects are loaded in memory (and hence
    #    _undefined_ during static linking)
    # The build will fail with errors like this:
    #    "Undefined symbols for architecture x86_64"
    # with various LLVM symbols being undefined. Since those symbols are later
    # loaded and resolved at runtime, these errors are false positives.
    # This behaviour can be modified via the '-undefined' OS X linker flag as
    # follows.
    target_link_libraries(
      ${plugin}
      "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>"
      )
endforeach()
