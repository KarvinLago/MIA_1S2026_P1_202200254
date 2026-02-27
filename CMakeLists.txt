cmake_minimum_required(VERSION 3.10)
project(MIA_P1)

set(CMAKE_CXX_STANDARD 17)

# add_executable(MIA_P1
#     main.cpp
#     Analyzer/Analyzer.cpp
#     Disk/DiskManager.cpp
#     Utilities/FileUtils.cpp
# )

add_executable(MIA_P1
    main.cpp
    Analyzer/Analyzer.cpp
    Disk/DiskManager.cpp
    Utilities/FileUtils.cpp
    Mount/MountManager.cpp
    FileSystem/MkfsManager.cpp
    Auth/LoginManager.cpp
    Users/GroupManager.cpp
    Users/UserManager.cpp
)