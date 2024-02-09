interpreterlib_sources = [
  'src/parser.c',
  'src/interpreter.c',
  'src/obj.c',
  'src/env.c',
]

interpreter_sources = [
  'src/main.c',
]

interpreterlibinc = [lexerinc, baseinc]
interpreterlib = library('kokosinterpreter',
                         interpreterlib_sources,
                         include_directories : interpreterlibinc,
                         link_with : lexerlib)

interpreterinc = include_directories('src')

executable('kokosi',
           interpreter_sources,
           link_with : interpreterlib,
           include_directories : interpreterlibinc)
