project('openroll', 'cpp', version: '0.17.2', default_options: ['cpp_std=c++17', 'warning_level=2'])

windows = host_machine.system() == 'windows'
sdk_root = get_option('sdk_root')
if sdk_root == ''
  error('Pass -Dsdk_root=/absolute/path/to/BlackmagicRAWSDK to use the vendor headers.')
endif

if windows
  cpp = meson.get_compiler('cpp')
  if cpp.get_id() != 'msvc'
    error('The Windows port uses MSVC and the Microsoft IDL compiler.')
  endif
  generated = get_option('windows_braw_headers')
  vlc_root = get_option('windows_vlc_sdk')
  if generated == '' or vlc_root == ''
    error('Pass -Dwindows_braw_headers and -Dwindows_vlc_sdk, or run scripts/build-windows.ps1.')
  endif
  sdk_headers = include_directories(generated, is_system: true)
  sdk_libraries = sdk_root / 'Win' / 'Libraries'
  vlc = declare_dependency(
    include_directories: include_directories(vlc_root / 'include', is_system: true),
    dependencies: cpp.find_library('libvlc', dirs: [vlc_root / 'lib']),
  )
  qt = [
    dependency('qt6', modules: ['Core', 'Gui', 'Widgets', 'Concurrent', 'Multimedia', 'Sql'], method: 'qmake'),
    vlc,
    cpp.find_library('oleaut32'),
  ]
  gui_deps = [dependency('qt6', modules: ['Widgets'], method: 'qmake', main: true)]
  platform_args = ['/utf-8', '/DNOMINMAX']
  add_project_arguments(platform_args, language: 'cpp')
  executable_args = []
else
  qt = [
    dependency('Qt6Widgets', method: 'pkg-config'),
    dependency('Qt6Concurrent', method: 'pkg-config'),
    dependency('Qt6Multimedia', method: 'pkg-config'),
    dependency('Qt6Sql', method: 'pkg-config'),
    dependency('libvlc', version: ['>=3.0', '<4.0'], method: 'pkg-config'),
  ]
  sdk_headers = include_directories(sdk_root / 'Linux' / 'Include', is_system: true)
  sdk_libraries = sdk_root / 'Linux' / 'Libraries'
  executable_args = ['-fPIC']
  gui_deps = []
endif
raw = static_library('openroll-raw', 'src/raw.cpp', dependencies: qt, include_directories: sdk_headers)
playback = static_library('openroll-playback', ['src/playback.cpp', 'src/vlc.cpp'], dependencies: qt)
ingest_engine = static_library('openroll-ingest', 'src/ingest.cpp', dependencies: qt)
project_store = static_library('openroll-project', ['src/project.cpp', 'src/media_scan.cpp'], dependencies: qt)
proxy_engine = static_library('openroll-proxy', 'src/proxy.cpp', dependencies: qt)
app = executable('openroll', ['src/main.cpp', 'src/ingest_widget.cpp', 'src/media.cpp', 'src/new_project.cpp', 'src/proxy_widget.cpp'], link_with: [playback, project_store, proxy_engine, raw, ingest_engine], dependencies: qt + gui_deps, cpp_args: executable_args, win_subsystem: 'windows', install: true)
test('window and workspace navigation', app, args: ['--smoke-test'], env: ['QT_QPA_PLATFORM=offscreen'])
test('Media and Ingest drops persist in the active project', app, args: ['--media-drop-test'],
  env: ['QT_QPA_PLATFORM=offscreen'], timeout: 15)
braw_test = executable('braw-test', 'tests/braw.cpp', include_directories: include_directories('src'),
  link_with: [playback, raw], dependencies: qt, cpp_args: executable_args)
fixture = get_option('braw_test_clip')
if fixture != ''
  test('BRAW rendering and transport', braw_test, args: [fixture, sdk_libraries],
    env: ['QT_QPA_PLATFORM=offscreen'], timeout: 90)
endif

executable('vlc-test', 'tests/vlc.cpp', include_directories: include_directories('src'),
  link_with: [playback, raw], dependencies: qt, cpp_args: executable_args)

ingest_test = executable('ingest-test', 'tests/ingest.cpp', include_directories: include_directories('src'),
  link_with: ingest_engine, dependencies: qt, cpp_args: executable_args)
test('Verified ingest and failure recovery', ingest_test, timeout: 30)

project_test = executable('project-test', 'tests/project.cpp', include_directories: include_directories('src'),
  link_with: [project_store, raw, ingest_engine], dependencies: qt, cpp_args: executable_args)
project_fixtures = []
if fixture != ''
  project_fixtures = [fixture, sdk_libraries]
endif
test('Media scan and project persistence', project_test, args: project_fixtures, timeout: 50)

proxy_test = executable('proxy-test', 'tests/proxy.cpp', include_directories: include_directories('src'),
  link_with: [proxy_engine, raw], dependencies: qt, cpp_args: executable_args)
if fixture != ''
  test('BRAW to ProRes Proxy export', proxy_test, args: project_fixtures, timeout: 120)
endif

new_project_test = executable('new-project-test', ['tests/new_project.cpp', 'src/new_project.cpp'],
  include_directories: include_directories('src'), dependencies: qt, cpp_args: executable_args)
test('New project form validation', new_project_test, env: ['QT_QPA_PLATFORM=offscreen'], timeout: 10)

playback_time_test = executable('playback-time-test', 'tests/playback_time.cpp',
  include_directories: include_directories('src'), dependencies: qt, cpp_args: executable_args)
test('Playback timecode formatting', playback_time_test)

transcode_project_test = executable('transcode-project-test',
  ['tests/transcode_project.cpp', 'src/proxy_widget.cpp'],
  include_directories: include_directories('src'),
  link_with: [project_store, proxy_engine, raw], dependencies: qt, cpp_args: executable_args)
test('Transcode follows the active project', transcode_project_test, env: ['QT_QPA_PLATFORM=offscreen'])

ingest_link_test = executable('ingest-link-test', ['tests/ingest_link.cpp', 'src/ingest_widget.cpp'],
  include_directories: include_directories('src'), dependencies: qt, cpp_args: executable_args,
  link_with: [project_store, ingest_engine, raw])
test('Dropping a folder links the original source to its project', ingest_link_test,
  env: ['QT_QPA_PLATFORM=offscreen'], timeout: 15)
test('Dropping a folder without a project asks where to link it', ingest_link_test,
  args: ['--no-project'], env: ['QT_QPA_PLATFORM=offscreen'], timeout: 15)
test('Dropping an individual clip links that clip', ingest_link_test,
  args: ['--single-file'], env: ['QT_QPA_PLATFORM=offscreen'], timeout: 15)
