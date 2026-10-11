function(p2p_add_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE p2p_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

p2p_add_test(tst_codegenerator)
p2p_add_test(tst_filenames)
p2p_add_test(tst_models)
