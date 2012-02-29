PROJECT (dgc)

add_subdirectory (dfjson)

# WTF
include_directories (
    .
    dfjson/
    dfjson/include
    ${CMAKE_SOURCE_DIR}
)

# Some globbing
FILE(GLOB PROJECT_SRCS *.cpp)
FILE(GLOB PROJECT_HDRS *.h)

FILE(GLOB DJS_SRCS dfjson/*.cpp)
LIST(APPEND PROJECT_SRCS ${DJS_SRCS})

SET_SOURCE_FILES_PROPERTIES( ${PROJECT_HDRS} PROPERTIES HEADER_FILE_ONLY TRUE)

SET(PROJECT_LIBS
    ${PROJECT_LIBS}
)

# mash them together (headers are marked as headers and nothing will try to compile them)
LIST(APPEND PROJECT_SRCS ${PROJECT_HDRS})

#linux
IF(UNIX)
    add_definitions(-DLINUX_BUILD)
    SET(PROJECT_LIBS
        # add any extra linux libs here
        ${PROJECT_LIBS}
    )
# windows
ELSE(UNIX)
    SET(PROJECT_LIBS
        # add any extra linux libs here
        ${PROJECT_LIBS}
        $(NOINHERIT)
    )
ENDIF(UNIX)
# this makes sure all the stuff is put in proper places and linked to dfhack
DFHACK_PLUGIN(dgc ${PROJECT_SRCS} LINK_LIBRARIES ${PROJECT_LIBS})

install(DIRECTORY gui/
        DESTINATION "dgc"
        PATTERN "CVS" EXCLUDE
        PATTERN ".svn" EXCLUDE
        PATTERN ".git" EXCLUDE)

