Import("env")

# Slip -Wno-deprecated-copy in only for C++ builds.
env.Append(CXXFLAGS=["-Wno-deprecated-copy"])
