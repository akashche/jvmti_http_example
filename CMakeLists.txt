
cmake_minimum_required(VERSION 3.0)

project ( jvmti1 CXX )

# pion specific compile flags for both targets
add_compile_options ( -g --std=c++11 -fPIC -Wall -Werror -Wextra )
add_definitions ( -DASIO_STANDALONE -DASIO_HAS_STD_CHRONO )

# pion http server lib
option ( STATICLIB_PION_USE_LOG4CPLUS "Use log4cplus" OFF )
option ( STATICLIB_PION_DISABLE_LOGGING "Disable logging to std out and err" OFF )
option ( STATICLIB_PION_USE_OPENSSL "Use OpenSSL" OFF )
file ( GLOB_RECURSE pion_SRC src/pion/*.cpp )
add_library ( pion STATIC ${pion_SRC} )
target_include_directories ( pion BEFORE PRIVATE include )

# agent
add_library ( ${PROJECT_NAME} SHARED src/jvmti1.cpp )
target_include_directories ( ${PROJECT_NAME} BEFORE PRIVATE 
        include
        /usr/lib/jvm/java-openjdk/include 
        /usr/lib/jvm/java-openjdk/include/linux )
target_link_libraries ( ${PROJECT_NAME} pion -pthread )

# test
# run: javac -d . ../test/App.java && java -agentpath:`pwd`/libjvmti1.so App
#enable_testing ( )
#get_property ( ${PROJECT_NAME}_LOCATION TARGET ${PROJECT_NAME} PROPERTY LOCATION)
#message ( STATUS $<TARGET_FILE:${PROJECT_NAME}> )
#add_test ( ${PROJECT_NAME}_test /usr/bin/javac -d . ${CMAKE_CURRENT_LIST_DIR}/test/App.java && /usr/bin/java -agentpath:$<TARGET_FILE:${PROJECT_NAME}> )

#add_executable ( uptime_agent src/uptime/main.cpp )
#target_link_libraries ( uptime_agent pion -pthread )
#target_include_directories ( uptime_agent BEFORE PRIVATE include )
#target_compile_options ( uptime_agent PRIVATE -g --std=c++11 -fPIC -Wall -Werror -Wextra )
#target_compile_definitions ( uptime_agent PRIVATE -DASIO_STANDALONE -DASIO_HAS_STD_CHRONO )