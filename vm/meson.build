vm_sources = [
  'src/main.c',
  'src/parser.c',
]

executable('kokosvm',
  vm_sources,
  include_directories : [lexerinc, baseinc],
  link_with : [lexerlib],
  c_args: ['-DBASE_IMPL', '-DBASE_STATIC'])
