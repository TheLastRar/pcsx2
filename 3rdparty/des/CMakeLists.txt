add_library(des STATIC src/des.cpp include/des.h)

target_include_directories(des PUBLIC include)
target_include_directories(des PRIVATE include)
