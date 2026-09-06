project('thermalcamera', 'cpp', 'objcpp',
    version: '1.0.0',
    default_options: ['warning_level=3', 'cpp_std=c++17', 'optimization=3', 'b_lto=true']
)

thread_dep = dependency('threads')

# Compiler optimizations
cpp = meson.get_compiler('cpp')
extra_args = []
if cpp.has_argument('-march=native')
    extra_args += ['-march=native']
endif

sdl2_dep = dependency('sdl2', version: '>=2.0.0', required: false)

if host_machine.system() == 'darwin'
    # No libuvc dependency on macOS as we use AVFoundation/IOKit
    apple_frameworks = dependency('appleframeworks',
        modules: ['AVFoundation', 'CoreMedia', 'CoreMediaIO', 'CoreVideo', 'IOKit', 'Foundation'])

    src_files = files(
        'src/render/Renderer.cpp',
        'src/thermal/ThermalProcessor.cpp',
        'src/camera/ICamera.hpp',
        'src/camera/Camera.hpp',
        'src/camera/ObjCPtr.hpp',
        'src/camera/MacCameraImpl.mm',
        'src/camera/MacCameraProvider.mm',
        'src/camera/MacAVFoundationStreamer.mm',
        'src/camera/MacIOKitUvcController.cpp',
        'src/main.cpp',
    )
else
    uvc_dep = dependency('libuvc', version: '>=0.0.6', required: false)
    if not uvc_dep.found()
        # Fall back to vendored libuvc (pinned git master, see subprojects/libuvc.wrap)
        uvc_sub = subproject('libuvc', required: true)
        uvc_dep = uvc_sub.get_variable('uvc_dep')
    endif

    src_files = files(
        'src/render/Renderer.cpp',
        'src/thermal/ThermalProcessor.cpp',
        'src/camera/ICamera.hpp',
        'src/camera/Camera.hpp',
        'src/camera/UvcCameraImpl.cpp',
        'src/camera/UvcCameraProvider.cpp',
        'src/main.cpp',
    )
endif

if host_machine.system() == 'darwin'
    exe = executable('thermalcamera',
        src_files,
        dependencies: [sdl2_dep, thread_dep, apple_frameworks],
        objc_args: ['-fobjc-arc'],
        objcpp_args: ['-fobjc-arc', '-std=c++17'] + extra_args,
        cpp_args: ['-std=c++17'] + extra_args,
        link_args: ['-lobjc', '-Wl,-framework,Foundation'],
        install: false
    )
else
    exe = executable('thermalcamera',
        src_files,
        dependencies: [sdl2_dep, uvc_dep, thread_dep],
        install: true,
        install_dir: 'bin',
        cpp_args: ['-std=c++17'] + extra_args
    )
endif

if host_machine.system() == 'darwin'
    plist_data = configuration_data()
    plist_data.set('BUNDLE_ID', 'com.thermalcamera.app')

    configure_file(
        input: 'macos/Info.plist.in',
        output: 'Info.plist',
        configuration: plist_data
    )

    custom_target(
        'app_bundle',
        output: 'ThermalCamera.app',
        depends: exe,
        command: [
            'mkdir', '-p', '@OUTDIR@/ThermalCamera.app/Contents/MacOS',
            '&&', 'mkdir', '-p', '@OUTDIR@/ThermalCamera.app/Contents/Resources',
            '&&', 'cp', meson.current_build_dir() / 'thermalcamera', '@OUTDIR@/ThermalCamera.app/Contents/MacOS/thermalcamera',
            '&&', 'cp', meson.current_build_dir() / 'Info.plist', '@OUTDIR@/ThermalCamera.app/Contents/',
            '&&', 'chmod', '+x', '@OUTDIR@/ThermalCamera.app/Contents/MacOS/thermalcamera'
        ],
        install: true,
        install_dir: 'Applications',
        build_by_default: true
    )
endif

test_exe = executable('test_thermal',
    files('src/tests/test_thermal.cpp', 'src/thermal/ThermalProcessor.cpp'),
    dependencies: [thread_dep],
    cpp_args: ['-std=c++17']
)
test('thermal_tests', test_exe)

test_renderer = executable('test_renderer',
    files('src/tests/test_renderer.cpp', 'src/render/Renderer.cpp', 'src/thermal/ThermalProcessor.cpp'),
    dependencies: [sdl2_dep, thread_dep],
    cpp_args: ['-std=c++17']
)
# Note: Renderer test is interactive, so maybe not add to 'test()' suite unless it has headless mode
# But for now I'll add it so I can run it manually or via meson test if I add a flag
# The test auto-quits after 5 seconds, so it's safe to add.
test('renderer_test', test_renderer)
