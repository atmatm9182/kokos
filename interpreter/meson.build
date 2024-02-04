interpreter_sources = ['src/main.c', 'src/parser.c', 'src/interpreter.c', 'src/obj.c']
interpreter_includes = [lexerinc, baseinc]
executable('kokosi',
           interpreter_sources,
           link_with : lexerlib,
           include_directories : interpreter_includes)
