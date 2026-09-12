
option(NX_MODULE_STORE_APP_STORE "Build the Apple App Store (StoreKit) storefront backend" OFF)

if (NOT NX_MODULE_STORE_APP_STORE)
    return()
endif ()

# nx_module()'s own PLATFORMS gate runs too late to help here - a project
# that also builds for Windows/Linux/macOS/Android still passes
# -DNX_MODULE_STORE_APP_STORE=ON for this platform-irrelevant module (its
# module list isn't per-platform), so this must skip itself before ever
# reaching that gate, the same "skipped on <platform>" outcome
# _nx_module_gate would otherwise report.
if (NOT IOS)
    message(STATUS "nx2d: module 'store_app_store' skipped on this platform")
    return()
endif ()

# No vendor SDK to resolve here - StoreKit ships with the iOS SDK already
# installed alongside Xcode, the same "nothing to vendor" shape
# store_microsoft's Windows.Services.Store already established for a
# platform-bundled storefront API. Unlike the JNI-shim mobile backends,
# there is no separate shim language at all: Objective-C++ lets this
# module's own .mm files call StoreKit directly, so this CMakeLists.txt
# builds the module's entire implementation, not just a C++ half of some
# other-language boundary.
set_source_files_properties(
        store_app_store_platform.mm store_app_store_services.mm
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc")

nx_module(store_app_store
        DISPLAY "Apple App Store (StoreKit) storefront backend"
        SOURCES store_app_store_module.cpp store_app_store_platform.mm
                store_app_store_services.mm
        DEPENDS nx_module_store "-framework StoreKit" "-framework Foundation"
        PLATFORMS ios)
