# cmake_minimum_required(VERSION 3.10)
# project(MIA_P1)

# set(CMAKE_CXX_STANDARD 17)

# add_executable(MIA_P1
#     main.cpp
#     Analyzer/Analyzer.cpp
#     Disk/DiskManager.cpp
#     Utilities/FileUtils.cpp
#     Mount/MountManager.cpp
#     FileSystem/MkfsManager.cpp
#     FileSystem/BlockManager.cpp
#     FileSystem/CatManager.cpp
#     FileSystem/MkdirManager.cpp
#     FileSystem/MkfileManager.cpp
#     Auth/LoginManager.cpp
#     Users/GroupManager.cpp
#     Users/UserManager.cpp
# )

cmake_minimum_required(VERSION 3.10)
project(MIA_P1)
set(CMAKE_CXX_STANDARD 17)

# OpenSSL requerido por httplib
find_package(OpenSSL REQUIRED)

add_executable(MIA_P1
    main.cpp
    Analyzer/Analyzer.cpp
    Disk/DiskManager.cpp
    Utilities/FileUtils.cpp
    Mount/MountManager.cpp
    FileSystem/MkfsManager.cpp
    FileSystem/BlockManager.cpp
    FileSystem/CatManager.cpp
    FileSystem/MkdirManager.cpp
    FileSystem/MkfileManager.cpp
    Auth/LoginManager.cpp
    Users/GroupManager.cpp
    Users/UserManager.cpp
)

target_include_directories(MIA_P1 PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(MIA_P1 PRIVATE OpenSSL::SSL OpenSSL::Crypto pthread)