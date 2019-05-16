deps = [
  dependency('gio-2.0', version: '>= 2.31.0'),
  dependency('glib-2.0', version: '>= 2.31.0'),
  dependency('gobject-2.0', version: '>= 2.31.0'),
  dependency('json-glib-1.0', version: '>= 1.1.1'),
  cc.find_library('m', required: false),
  libwalbottle_dep,
  libwalbottle_utils_dep,
]

envs = test_env + [
  'G_TEST_SRCDIR=' + meson.current_source_dir(),
  'G_TEST_BUILDDIR=' + meson.current_build_dir(),
]

# Helper library.
libwalbottle_test_utils_sources = [
  'utils.c',
  'utils.h',
]

libwalbottle_test_utils = static_library('walbottle-utilities',
  libwalbottle_test_utils_sources,
  dependencies: deps,
  c_args: [
    '-DG_LOG_DOMAIN="libwalbottle-tests"',
  ],
  include_directories: root_inc,
  install: false,
)
libwalbottle_test_utils_dep = declare_dependency(
  link_with: libwalbottle_test_utils,
  include_directories: root_inc,
)

# Tests
test_programs = [
  'schema',
  'schema-keywords',
  'self-hosting',
  'string-set',
]

# FIXME: Install .json files for installed-tests
installed_tests_metadir = join_paths(datadir, 'installed-tests',
                                     'libwalbottle-' + libwalbottle_api_version)
installed_tests_execdir = join_paths(libexecdir, 'installed-tests',
                                     'libwalbottle-' + libwalbottle_api_version)

foreach program: test_programs
  test_conf = configuration_data()
  test_conf.set('installed_tests_dir', installed_tests_execdir)
  test_conf.set('program', program)

  configure_file(
    input: test_template,
    output: program + '.test',
    install: enable_installed_tests,
    install_dir: installed_tests_metadir,
    configuration: test_conf,
  )

  exe = executable(
    program,
    [program + '.c'],
    dependencies: deps + [libwalbottle_test_utils_dep],
    include_directories: root_inc,
    install: enable_installed_tests,
    install_dir: installed_tests_execdir,
  )

  test(
    program,
    exe,
    env: envs,
    args: ['--tap'],
  )
endforeach
