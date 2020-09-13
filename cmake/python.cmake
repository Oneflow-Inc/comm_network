find_package(Python3 COMPONENTS Interpreter REQUIRED)
message(STATUS "Python3 specified. Version found: " ${Python3_VERSION})
set(Python_EXECUTABLE ${Python3_EXECUTABLE})
message(STATUS "Using Python executable: " ${Python_EXECUTABLE})