# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# Checks that the man page documents every option the program's --help lists.
# cmake -DBINARY=<kurvenrausch> -DMAN=<kurvenrausch.6> -P check_man.cmake

execute_process(COMMAND "${BINARY}" --help OUTPUT_VARIABLE help RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "${BINARY} --help failed")
endif()
file(READ "${MAN}" man)
string(REGEX MATCHALL "--[a-z-]+" options "${help}")
list(REMOVE_DUPLICATES options)
list(LENGTH options count)
if(count LESS 5)
    message(FATAL_ERROR "found only ${count} options in --help")
endif()
foreach(option IN LISTS options)
    # In roff, every hyphen of an option is written as \-.
    string(REPLACE "-" "\\-" escaped "${option}")
    string(FIND "${man}" "${escaped}" found)
    if(found EQUAL -1)
        message(SEND_ERROR "the man page does not document ${option}")
    endif()
endforeach()
