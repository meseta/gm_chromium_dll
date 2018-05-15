Building libcef_dll_wrapper.lib and gm_chromium.dll
1. Download and compile CEF3: https://bitbucket.org/chromiumembedded/cef
    - download the Standard Distribution
    - Build libcef_dll_wrapper
2. Copy include/* from CEF3 to gm_chromium_dll/cef3/include/*
3. Copy the following files from CEF3 to gm_chromium_dll/cef3/lib:
    - Release/libcef.lib
    - Release/libcef.lib
    - libcef_dll_wrapper.lib and libcef_dll_wrapper.pdb from the libced_dll build directory
4. Build gm_chroimium.dll

Including gm_chroimium.dll in project.  The following files are needed:
    - gm_chroimium_dll/Release/gm_chromium.dll
    - cef_binary_x_windows32/Release/*
    - cef_binary_x_windows32/Resources/*