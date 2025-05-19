# CMake generated Testfile for 
# Source directory: /home/zangoose/Pubblici/ot-br-posix/tests/tools
# Build directory: /home/zangoose/Pubblici/ot-br-posix/tests/tools
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(pskc "/home/zangoose/Pubblici/ot-br-posix/tests/tools/test-pskc")
set_tests_properties(pskc PROPERTIES  ENVIRONMENT "OTBR_COMPUTER=/home/zangoose/Pubblici/ot-br-posix/tools/pskc" _BACKTRACE_TRIPLES "/home/zangoose/Pubblici/ot-br-posix/tests/tools/CMakeLists.txt;29;add_test;/home/zangoose/Pubblici/ot-br-posix/tests/tools/CMakeLists.txt;0;")
add_test(steering-data "/home/zangoose/Pubblici/ot-br-posix/tests/tools/test-steering-data")
set_tests_properties(steering-data PROPERTIES  ENVIRONMENT "OTBR_COMPUTER=/home/zangoose/Pubblici/ot-br-posix/tools/steering-data" _BACKTRACE_TRIPLES "/home/zangoose/Pubblici/ot-br-posix/tests/tools/CMakeLists.txt;37;add_test;/home/zangoose/Pubblici/ot-br-posix/tests/tools/CMakeLists.txt;0;")
