project('kokos', 'c', default_options : ['werror=true'])

add_global_arguments('-Og', '-Wno-unused-function', '-Wno-unused-result', language : 'c')

baseinc = include_directories('include')

subdir('lexer')
subdir('interpreter')
subdir('vm')
subdir('tests')
