add_library(arm64ecfex SHARED
  Module.cpp
  Module.S
  libarm64ecfex.def
  $<TARGET_OBJECTS:FEXCore_object>)

patch_library_wine(arm64ecfex)

target_include_directories(arm64ecfex PRIVATE
  "${CMAKE_SOURCE_DIR}/Source/Windows/include/"
  "${CMAKE_SOURCE_DIR}/Source/"
  "${CMAKE_SOURCE_DIR}/Source/Windows/")

target_link_libraries(arm64ecfex PRIVATE
  FEXCore_Base
  Common
  CommonTools
  CommonWindows
  CommonWindowsRuntime
  ntdll_ex)

target_link_options(arm64ecfex PRIVATE -static -nostdlib -nostartfiles -nodefaultlibs -lc++ -lc++abi -lunwind
  # proton-mac: 16KB PE section+file alignment so the module's RX sections can be exec-mapped on
  # Apple Silicon's 16KB host pages (Wine's map_image_into_view can't set PROT_EXEC on a 16KB page
  # that packs sub-4KB sections with mixed protections). Wine's own builtins are 16KB-aligned; FEX's
  # default 4KB alignment fails to load. Same fix we apply to guest test exes.
  -Wl,--file-alignment,0x4000 -Wl,--section-alignment,0x4000)
target_link_libraries(arm64ecfex PRIVATE ${LIBGCC_PATH})
install(TARGETS arm64ecfex RUNTIME
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
  COMPONENT Runtime)
