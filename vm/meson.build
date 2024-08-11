vm_sources = [
  'src/main.c',
  'src/parser.c',
  'src/vm.c',
  'src/compile.c',
  'src/instruction.c',
  'src/native.c',
  'src/gc.c',
  'src/runtime.c',
  'src/hash.c',
  'src/value.c',
  'src/string-store.c',
  'src/scope.c',
]

kokosvm_cargs = ['-Wno-unused-value', '-DBASE_IMPLEMENTATION', '-DBASE_STATIC']

if get_option('buildtype') == 'debug'
  kokosvm_cargs += '-DKOKOS_DEBUG_BUILD'
endif

executable('kokosvm',
  vm_sources,
  include_directories : [lexerinc, baseinc],
  link_with : [lexerlib],
  c_args: kokosvm_cargs)
