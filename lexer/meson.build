lexer_sources = ['src/lexer.c']

lexerlibinc = [baseinc]

lexerinc = include_directories('src')
lexerlib = static_library(
  'kokoslexer',
  lexer_sources,
  include_directories : lexerlibinc,
  c_args: ['-DBASE_IMPLEMENTATION', '-DBASE_STATIC'])
