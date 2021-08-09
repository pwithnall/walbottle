# Internal utility helper library
libwalbottle_utils_sources = [
  'wbl-json-node.h',
  'wbl-json-node.c',
  'wbl-string-set.h',
  'wbl-string-set.c',
]

libwalbottle_utils_deps = [
  dependency('gio-2.0', version: '>= 2.34.0'),
  dependency('glib-2.0', version: '>= 2.34.0'),
  dependency('gobject-2.0', version: '>= 2.31.0'),
  dependency('json-glib-1.0', version: '>= 1.1.1'),
]

libwalbottle_utils = static_library('walbottle-utils',
  libwalbottle_utils_sources,
  dependencies: libwalbottle_utils_deps,
  c_args: [
    '-DG_LOG_DOMAIN="libwalbottle"',
  ],
  include_directories: root_inc,
  install: false,
)
libwalbottle_utils_dep = declare_dependency(
  link_with: libwalbottle_utils,
  include_directories: root_inc,
)

# libwalbottle library
libwalbottle_api_version = '0'
libwalbottle_api_name = 'walbottle-' + libwalbottle_api_version
libwalbottle_sources = [
  'wbl-meta-schema.c',
  'wbl-schema.c',
]
libwalbottle_nonmain_nongenerated_public_headers = [
  'wbl-meta-schema.h',
  'wbl-schema.h',
]

libwalbottle_main_public_header = configure_file(
  input: 'walbottle.h.in',
  output: 'walbottle.h',
  configuration: {
    'INCLUDES': '#include <libwalbottle/' + '>\n#include <libwalbottle/'.join(libwalbottle_nonmain_nongenerated_public_headers + ['wbl-version.h']) + '>\n',
  },
)

meta_schema_sources = gnome.compile_resources(
  'wbl-meta-schemas', 'wbl-meta-schemas.gresource.xml',
  source_dir: meson.current_source_dir(),
  c_name: 'wbl_meta_schemas',
)

version_array = meson.project_version().split('.')
wbl_version_h = configure_file(
  input: 'wbl-version.h.in',
  output: 'wbl-version.h',
  configuration: {
    'WBL_VERSION_MAJOR': version_array[0].to_int(),
    'WBL_VERSION_MINOR': version_array[1].to_int(),
    'WBL_VERSION_MICRO': version_array[2].to_int(),
  },
)

libwalbottle_public_headers = libwalbottle_nonmain_nongenerated_public_headers + [
  libwalbottle_main_public_header,
  wbl_version_h,
]

libwalbottle_public_deps = [
  dependency('gio-2.0', version: '>= 2.31.0'),
  dependency('glib-2.0', version: '>= 2.31.0'),
  dependency('gobject-2.0', version: '>= 2.31.0'),
  dependency('json-glib-1.0', version: '>= 1.1.1'),
]

# FIXME: Would be good to use subdir here: https://github.com/mesonbuild/meson/issues/2969
libwalbottle_include_subdir = join_paths('lib' + libwalbottle_api_name, 'walbottle')

mapfile = 'libwalbottle.map'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)

libwalbottle = library(libwalbottle_api_name,
  libwalbottle_sources + libwalbottle_public_headers + meta_schema_sources,
  dependencies: libwalbottle_public_deps + [
    libwalbottle_utils_dep,
    cc.find_library('m', required: false),
  ],
  c_args: [
    '-DG_LOG_DOMAIN="libwalbottle"',
  ],
  include_directories: root_inc,
  install: true,
  version: meson.project_version(),
  soversion: libwalbottle_api_version,
  link_args : ['-Wl,--no-undefined', vflag],
  link_depends : mapfile,
)
libwalbottle_dep = declare_dependency(
  link_with: libwalbottle,
  include_directories: root_inc,
)

# Public library bits.
install_headers(libwalbottle_public_headers,
  subdir: libwalbottle_include_subdir,
)

pkgconfig.generate(
  libraries: [ libwalbottle ],
  subdirs: libwalbottle_api_name,
  version: meson.project_version(),
  name: 'libwalbottle',
  filebase: 'lib' + libwalbottle_api_name,
  description: 'JSON unit test generation library',
  requires: libwalbottle_public_deps,
)

if get_option('introspection').auto() or get_option('introspection').enabled()
  gnome.generate_gir(libwalbottle,
    # FIXME: Use libwalbottle_public_headers
    sources: libwalbottle_sources + libwalbottle_nonmain_nongenerated_public_headers,
    nsversion: libwalbottle_api_version,
    # FIXME: Use libwalbottle_main_public_header instead; https://github.com/mesonbuild/meson/issues/5398
    #header: 'walbottle.h',
    namespace: 'Walbottle',
    symbol_prefix: 'wbl_',
    identifier_prefix: 'Wbl',
    export_packages: 'libwalbottle',
    includes: ['GObject-2.0', 'Gio-2.0', 'Json-1.0'],
    install: true,
    dependencies: libwalbottle_dep,
  )
endif

subdir('docs')
subdir('tests')
