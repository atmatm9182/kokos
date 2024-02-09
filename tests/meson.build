lex_test = executable('lexer-test',
           'lexer_test.c',
           link_with : lexerlib,
           include_directories : [lexerlibinc, lexerinc])

test('lexer test', lex_test)

interpreter_test = executable('interpreter-test',
           'interpreter_test.c',
           link_with : interpreterlib,
           include_directories : [interpreterlibinc, interpreterinc])

test('interpreter test', interpreter_test)
