"""Add or remove set(CMAKE_BUILD_TYPE "Debug") to all
CMakeList.txt files inside the src sub-directories
"""

import sys
import os
import os.path
import getopt


DEBUG_CMAKE_TXT = 'set(CMAKE_BUILD_TYPE "Debug")'


def check_text(filepath):
    """Checks if the file given contain the 
    [[DEBUG_CMAKE_TXT]] text.
    """

    f = open(filepath, "r")
    filetext = f.read()
    return DEBUG_CMAKE_TXT in filetext


def add_debug_cmd(filepath):
    """this function adds the cmake debug build command at
    the end of the file specified by the [filepath]
    """
    
    # only add the debug text if not already there
    if not check_text(filepath):
        f = open(filepath, "a+")
        f.write("\n" + DEBUG_CMAKE_TXT)
        f.close()


def rmv_debug_cmd(filepath):
    """this function undo's what add_debug_cmd did. It 
    removes the cmake debug command from the end of the
    file
    """

    RMV_CMD = "sed -i ':a;N;$!ba;s/\\n%s//g' %s"
    os.system(RMV_CMD % (DEBUG_CMAKE_TXT, filepath))


def run_cmd_on_all_cmake(run_foo):
    """Runs a specific function given as the argument 
    [[run_foo]] on all CMakeLists.txt inside all sub-dir
    in the src folder. The argument to [[run_foo]] is
    always the filepath to the CMakeLists.txt file.
    """

    src_dir = os.path.dirname(os.path.realpath(__file__))

    # iterate over all directories inside src
    for d in os.listdir(src_dir):
        curr_dir = os.path.join(src_dir, d)

        # skip all directories which dont have CMakeLists.txt
        if not os.path.isdir(curr_dir):
            continue
        psb_filepath_cmake = os.path.join(curr_dir, 'CMakeLists.txt')
        if not os.path.isfile(psb_filepath_cmake):
            continue

        print psb_filepath_cmake
        run_foo(psb_filepath_cmake)


def usage_help():
    print("""Usage:
python cmakedebug.py -a
    Adds 'set(CMAKE_BUILD_TYPE "Debug")' to all the CMakeList.txt files in the src sub-directories
python cmakedebug.py -r
    Removes 'set(CMAKE_BUILD_TYPE "Debug")' from all the CMakeList.txt files in the src sub-directories
python cmakedebug.py -h
    this help
""")


# main
if __name__ == "__main__":
    try:
        if len(sys.argv) <= 1:
            raise getopt.GetoptError("No arguments given")
        opts, args = getopt.getopt(sys.argv[1:], "har", ["help", "add", "remove"])
    except getopt.GetoptError:
        print("---Invalid usage---")
        usage_help()
        sys.exit(2)

    for opt, arg in opts:
        if opt in ("-h", "--help"):
            usage_help()
            sys.exit()
        elif opt in ("-a", "--add"):
            run_cmd_on_all_cmake(add_debug_cmd)
            sys.exit()
        elif opt in ("-r", "--remove"):
            run_cmd_on_all_cmake(rmv_debug_cmd)
            sys.exit()
