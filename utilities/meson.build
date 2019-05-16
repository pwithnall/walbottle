# All utilities have the same dependencies
deps = [
  dependency('gio-2.0', version: '>= 2.31.0'),
  dependency('glib-2.0', version: '>= 2.31.0'),
  dependency('gobject-2.0', version: '>= 2.31.0'),
  dependency('json-glib-1.0', version: '>= 1.1.1'),
  libwalbottle_dep,
]

# Internal utility helper library
libwalbottle_utilities_sources = [
  'wbl-utilities.c',
  'wbl-utilities.h',
]

libwalbottle_utilities = static_library('walbottle-utilities',
  libwalbottle_utilities_sources,
  dependencies: deps,
  c_args: [
    '-DG_LOG_DOMAIN="libwalbottle-utilities"',
  ],
  include_directories: root_inc,
  install: false,
)
libwalbottle_utilities_dep = declare_dependency(
  link_with: libwalbottle_utilities,
  include_directories: root_inc,
)

# json-validate utility
json_validate = executable('json-validate',
  ['json-validate.c'],
  dependencies: deps + [libwalbottle_utilities_dep],
  install: true,
  c_args: ['-DG_LOG_DOMAIN="json-validate"'],
  install_dir: bindir,
)
install_man('docs/json-validate.8')

# json-schema-validate utility
json_schema_validate = executable('json-schema-validate',
  ['json-schema-validate.c'],
  dependencies: deps + [libwalbottle_utilities_dep],
  install: true,
  c_args: ['-DG_LOG_DOMAIN="json-schema-validate"'],
  install_dir: bindir,
)
install_man('docs/json-schema-validate.8')

# json-schema-generate utility
json_schema_generate = executable('json-schema-generate',
  ['json-schema-generate.c'],
  dependencies: deps + [libwalbottle_utilities_dep],
  install: true,
  c_args: ['-DG_LOG_DOMAIN="json-schema-generate"'],
  install_dir: bindir,
)
install_man('docs/json-schema-generate.8')
