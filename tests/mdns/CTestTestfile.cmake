# CMake generated Testfile for 
# Source directory: /home/zangoose/Pubblici/ot-br-posix/tests/mdns
# Build directory: /home/zangoose/Pubblici/ot-br-posix/tests/mdns
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(mdns-single "/home/zangoose/Pubblici/ot-br-posix/tests/mdns/test-single")
set_tests_properties(mdns-single PROPERTIES  ENVIRONMENT "OTBR_MDNS=avahi;OTBR_TEST_MDNS=/home/zangoose/Pubblici/ot-br-posix/tests/mdns/otbr-test-mdns" _BACKTRACE_TRIPLES "/home/zangoose/Pubblici/ot-br-posix/tests/mdns/CMakeLists.txt;38;add_test;/home/zangoose/Pubblici/ot-br-posix/tests/mdns/CMakeLists.txt;0;")
add_test(mdns-multiple "/home/zangoose/Pubblici/ot-br-posix/tests/mdns/test-multiple")
set_tests_properties(mdns-multiple PROPERTIES  ENVIRONMENT "OTBR_MDNS=avahi;OTBR_TEST_MDNS=/home/zangoose/Pubblici/ot-br-posix/tests/mdns/otbr-test-mdns" _BACKTRACE_TRIPLES "/home/zangoose/Pubblici/ot-br-posix/tests/mdns/CMakeLists.txt;43;add_test;/home/zangoose/Pubblici/ot-br-posix/tests/mdns/CMakeLists.txt;0;")
add_test(mdns-update "/home/zangoose/Pubblici/ot-br-posix/tests/mdns/test-update")
set_tests_properties(mdns-update PROPERTIES  ENVIRONMENT "OTBR_MDNS=avahi;OTBR_TEST_MDNS=/home/zangoose/Pubblici/ot-br-posix/tests/mdns/otbr-test-mdns" _BACKTRACE_TRIPLES "/home/zangoose/Pubblici/ot-br-posix/tests/mdns/CMakeLists.txt;48;add_test;/home/zangoose/Pubblici/ot-br-posix/tests/mdns/CMakeLists.txt;0;")
add_test(mdns-stop "/home/zangoose/Pubblici/ot-br-posix/tests/mdns/test-stop")
set_tests_properties(mdns-stop PROPERTIES  ENVIRONMENT "OTBR_MDNS=avahi;OTBR_TEST_MDNS=/home/zangoose/Pubblici/ot-br-posix/tests/mdns/otbr-test-mdns" _BACKTRACE_TRIPLES "/home/zangoose/Pubblici/ot-br-posix/tests/mdns/CMakeLists.txt;53;add_test;/home/zangoose/Pubblici/ot-br-posix/tests/mdns/CMakeLists.txt;0;")
add_test(mdns-single-custom-host "/home/zangoose/Pubblici/ot-br-posix/tests/mdns/test-single-custom-host")
set_tests_properties(mdns-single-custom-host PROPERTIES  ENVIRONMENT "OTBR_MDNS=avahi;OTBR_TEST_MDNS=/home/zangoose/Pubblici/ot-br-posix/tests/mdns/otbr-test-mdns" _BACKTRACE_TRIPLES "/home/zangoose/Pubblici/ot-br-posix/tests/mdns/CMakeLists.txt;58;add_test;/home/zangoose/Pubblici/ot-br-posix/tests/mdns/CMakeLists.txt;0;")
add_test(mdns-multiple-custom-hosts "/home/zangoose/Pubblici/ot-br-posix/tests/mdns/test-multiple-custom-hosts")
set_tests_properties(mdns-multiple-custom-hosts PROPERTIES  ENVIRONMENT "OTBR_MDNS=avahi;OTBR_TEST_MDNS=/home/zangoose/Pubblici/ot-br-posix/tests/mdns/otbr-test-mdns" _BACKTRACE_TRIPLES "/home/zangoose/Pubblici/ot-br-posix/tests/mdns/CMakeLists.txt;63;add_test;/home/zangoose/Pubblici/ot-br-posix/tests/mdns/CMakeLists.txt;0;")
add_test(mdns-service-subtypes "/home/zangoose/Pubblici/ot-br-posix/tests/mdns/test-service-subtypes")
set_tests_properties(mdns-service-subtypes PROPERTIES  ENVIRONMENT "OTBR_MDNS=avahi;OTBR_TEST_MDNS=/home/zangoose/Pubblici/ot-br-posix/tests/mdns/otbr-test-mdns" _BACKTRACE_TRIPLES "/home/zangoose/Pubblici/ot-br-posix/tests/mdns/CMakeLists.txt;68;add_test;/home/zangoose/Pubblici/ot-br-posix/tests/mdns/CMakeLists.txt;0;")
add_test(mdns-single-empty-service-name "/home/zangoose/Pubblici/ot-br-posix/tests/mdns/test-single-empty-service-name")
set_tests_properties(mdns-single-empty-service-name PROPERTIES  ENVIRONMENT "OTBR_MDNS=avahi;OTBR_TEST_MDNS=/home/zangoose/Pubblici/ot-br-posix/tests/mdns/otbr-test-mdns" _BACKTRACE_TRIPLES "/home/zangoose/Pubblici/ot-br-posix/tests/mdns/CMakeLists.txt;73;add_test;/home/zangoose/Pubblici/ot-br-posix/tests/mdns/CMakeLists.txt;0;")
