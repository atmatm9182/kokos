project('kokos', 'c', default_options : ['werror=true', 'warning_level=1'])

add_global_arguments('-Wno-unused-function', '-Wno-unused-result', language : 'c')

baseinc = include_directories('include')

subdir('lexer')
subdir('interpreter')
subdir('vm')
subdir('tests')
