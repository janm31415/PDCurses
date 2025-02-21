cmake_minimum_required(VERSION 3.10)
project(PDCurses)

set(PDCURSES_TARGET "wincon" CACHE STRING "Choose your target environment")
set_property(CACHE PDCURSES_TARGET PROPERTY STRINGS wincon sdl2 x11)

set(HDRS
curses.h
curspriv.h
panel.h
)

if (${PDCURSES_TARGET} STREQUAL "wincon")
list(APPEND HDRS wincon/pdcwin.h)
endif (${PDCURSES_TARGET} STREQUAL "wincon")

if (${PDCURSES_TARGET} STREQUAL "sdl2")
list(APPEND HDRS sdl2/pdcsdl.h)
endif (${PDCURSES_TARGET} STREQUAL "sdl2")

if (${PDCURSES_TARGET} STREQUAL "x11")
list(APPEND HDRS x11/pdcx11.h)
endif (${PDCURSES_TARGET} STREQUAL "x11")
	
if (UNIX)

set(XLIBINCLUDE "/usr/include/X11")
set(XLIBLIBRARY "/usr/lib/x86_64-linux-gnu/libXt.so" "/usr/lib/x86_64-linux-gnu/libXmu.so" "/usr/lib/x86_64-linux-gnu/libXpm.so" "/usr/lib/x86_64-linux-gnu/libXaw.so" "X11")

if (APPLE)
set(XLIBINCLUDE "/usr/X11/include/X11" "/usr/X11/include")
set(XLIBLIBRARY "/usr/X11/lib/libX11.dylib" "/usr/X11/lib/libXt.dylib" "/usr/X11/lib/libXmu.dylib" "/usr/X11/lib/libXpm.dylib" "/usr/X11/lib/libXaw.dylib")
endif (APPLE)

endif(UNIX)
	
	
set(SRCS
pdcurses/addch.c
pdcurses/addchstr.c
pdcurses/addstr.c
pdcurses/attr.c
pdcurses/beep.c
pdcurses/bkgd.c
pdcurses/border.c
pdcurses/clear.c
pdcurses/color.c
pdcurses/debug.c
pdcurses/delch.c
pdcurses/deleteln.c
pdcurses/getch.c
pdcurses/getstr.c
pdcurses/getyx.c
pdcurses/inch.c
pdcurses/inchstr.c
pdcurses/initscr.c
pdcurses/inopts.c
pdcurses/insch.c
pdcurses/insstr.c
pdcurses/instr.c
pdcurses/kernel.c
pdcurses/keyname.c
pdcurses/mouse.c
pdcurses/move.c
pdcurses/outopts.c
pdcurses/overlay.c
pdcurses/pad.c
pdcurses/panel.c
pdcurses/printw.c
pdcurses/refresh.c
pdcurses/scanw.c
pdcurses/scroll.c
pdcurses/scr_dump.c
pdcurses/slk.c
pdcurses/termattr.c
pdcurses/touch.c
pdcurses/util.c
pdcurses/window.c
${PDCURSES_TARGET}/pdcclip.c
${PDCURSES_TARGET}/pdcdisp.c
${PDCURSES_TARGET}/pdcgetsc.c
${PDCURSES_TARGET}/pdckbd.c
${PDCURSES_TARGET}/pdcscrn.c
${PDCURSES_TARGET}/pdcsetsc.c
${PDCURSES_TARGET}/pdcutil.c
)

if (${PDCURSES_TARGET} STREQUAL "x11")
list(APPEND SRCS x11/sb.c)
list(APPEND SRCS x11/scrlbox.c)
list(APPEND HDRS x11/scrlbox.h)
endif (${PDCURSES_TARGET} STREQUAL "x11")

set(FIREWORK
demos/firework.c
)

set(RAIN
demos/rain.c
)

set(WORM
demos/worm.c
)

set(OZDEMO
demos/ozdemo.c
)

set(XMAS
demos/xmas.c
)

set(TUIDEMO
demos/tuidemo.c
demos/tui.c
demos/tui.h
)

set(PTEST
demos/ptest.c
)

set(TESTCURS
demos/testcurs.c
)

if (WIN32)
set(CMAKE_C_FLAGS_DEBUG "/W4 /MP /GF /RTCu /Od /MDd /Zi")
set(CMAKE_CXX_FLAGS_DEBUG "/W4 /MP /GF /RTCu /Od /MDd /Zi")
set(CMAKE_C_FLAGS_RELEASE "/W4 /MP /GF /O2 /Ob2 /Oi /Ot /MD /Zi /DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "/W4 /MP /GF /O2 /Ob2 /Oi /Ot /MD /Zi /DNDEBUG")
endif (WIN32)

# general build definitions
#add_definitions(-D_UNICODE)
#add_definitions(-DUNICODE)
add_definitions(-D_SCL_SECURE_NO_WARNINGS)

if (WIN32)
add_definitions(-DPDC_DLL_BUILD)
endif (WIN32)
add_definitions(-DPDC_RGB)
add_definitions(-DPDC_FORCE_UTF8)
add_definitions(-DPDC_WIDE)

add_library(pdcurses SHARED ${HDRS} ${SRCS})
source_group("Header Files" FILES ${hdrs})
source_group("Source Files" FILES ${srcs})

target_include_directories(pdcurses
    PUBLIC
   	${CMAKE_CURRENT_SOURCE_DIR}
    )	
	
if (${PDCURSES_TARGET} STREQUAL "x11")
target_include_directories(pdcurses
    PUBLIC
   	${XLIBINCLUDE}
    )	
target_link_libraries(pdcurses
    PUBLIC
	  ${XLIBLIBRARY}  
    )	
endif(${PDCURSES_TARGET} STREQUAL "x11")

if (${PDCURSES_TARGET} STREQUAL "sdl2")
target_include_directories(pdcurses
    PUBLIC
   	${CMAKE_CURRENT_SOURCE_DIR}/../SDL2/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../SDL2_ttf
    )	
target_link_libraries(pdcurses
    PUBLIC
	SDL2
    SDL2_ttf
    )	
endif (${PDCURSES_TARGET} STREQUAL "sdl2")
    
    
add_executable(firework ${FIREWORK})
target_link_libraries(firework
    PUBLIC	    
    pdcurses
    )

add_executable(rain ${RAIN})
target_link_libraries(rain
    PUBLIC	    
    pdcurses
    )

add_executable(worm ${WORM})
target_link_libraries(worm
    PUBLIC	    
    pdcurses
    )

add_executable(ozdemo ${OZDEMO})
target_link_libraries(ozdemo
    PUBLIC	    
    pdcurses
    )

add_executable(xmas ${XMAS})
target_link_libraries(xmas
    PUBLIC	    
    pdcurses
    )

add_executable(tuidemo ${TUIDEMO})
target_link_libraries(tuidemo
    PUBLIC	    
    pdcurses
    )

add_executable(ptest ${PTEST})
target_link_libraries(ptest
    PUBLIC	    
    pdcurses
    )

add_executable(testcurs ${TESTCURS})
target_link_libraries(testcurs
    PUBLIC	    
    pdcurses
    )