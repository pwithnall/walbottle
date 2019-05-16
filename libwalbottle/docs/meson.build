if get_option('gtk_doc')
  ignore_headers = [
    'wbl-json-node.c',
    'wbl-json-node.h',
    'wbl-private.h',
    'wbl-string-set.c',
    'wbl-string-set.h',
    'utils.h',
  ]

  version_conf = configuration_data()
  version_conf.set('WALBOTTLE_VERSION', meson.project_version())
  version_xml = configure_file(
    input: 'version.xml.in',
    output: 'version.xml',
    configuration: version_conf,
  )

  gnome.gtkdoc('libwalbottle',
    main_xml: 'libwalbottle-docs.xml',
    module_version: libwalbottle_api_version,
    namespace: 'Wbl',
    src_dir: [ 'libwalbottle' ],
    dependencies: libwalbottle_dep,
    scan_args : [
      '--rebuild-types',
      '--ignore-headers=' + ' '.join(ignore_headers),
    ],
    content_files: [
      version_xml,
    ],
    install: true,
  )

  test('gtkdoc-check',
       gtkdoc_check,
       suite: ['libwalbottle', 'docs'],
       env: [
         'BUILDDIR=' + meson.current_build_dir(),
         'DOC_MODULE=libwalbottle',
         'DOC_MAIN_SGML_FILE=libwalbottle-docs.xml',
       ])
endif
