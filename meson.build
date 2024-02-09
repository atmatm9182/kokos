project('kokos', 'c', default_options : ['werror=true'])

add_global_arguments('-Og', language : 'c')

baseinc = include_directories('include')

subdir('lexer')
subdir('interpreter')
subdir('tests')
