qt_add_executable(tst_codegenerator tst_codegenerator.cpp)
target_link_libraries(tst_codegenerator PRIVATE p2p_core Qt6::Test)
add_test(NAME tst_codegenerator COMMAND tst_codegenerator)
