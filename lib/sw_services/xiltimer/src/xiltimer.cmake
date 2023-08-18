# Copyright (C) 2023 Advanced Micro Devices, Inc.  All rights reserved.
# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.3)

find_package(common)
list(APPEND TOTAL_TIMER_INSTANCES ${TMRCTR_NUM_DRIVER_INSTANCES})
list(APPEND TOTAL_TIMER_INSTANCES ${TTCPS_NUM_DRIVER_INSTANCES})
list(APPEND TOTAL_TIMER_INSTANCES ${SCUTIMER_NUM_DRIVER_INSTANCES})
list(LENGTH TMRCTR_NUM_DRIVER_INSTANCES CONFIG_AXI_TIMER)
list(LENGTH TTCPS_NUM_DRIVER_INSTANCES CONFIG_TTCPS)
list(LENGTH SCUTIMER_NUM_DRIVER_INSTANCES CONFIG_SCUTIMER)
list(LENGTH TOTAL_TIMER_INSTANCES _len)

if (NOT "${TOTAL_TIMER_INSTANCES}" STREQUAL "")
   set(XILTIMER_sleep_timer "Default;" CACHE STRING "This parameter is used to select specific timer for sleep functionality")
   SET_PROPERTY(CACHE XILTIMER_sleep_timer PROPERTY STRINGS "Default;${TOTAL_TIMER_INSTANCES}")

   set(XILTIMER_tick_timer "None;" CACHE STRING "This parameter is used to select specific timer for tick functionality")
   SET_PROPERTY(CACHE XILTIMER_tick_timer PROPERTY STRINGS "None;${TOTAL_TIMER_INSTANCES}")
else()
   set(XILTIMER_sleep_timer "Default" CACHE STRING "This parameter is used to select specific timer for sleep functionality")
   SET_PROPERTY(CACHE XILTIMER_sleep_timer PROPERTY STRINGS "Default")

    set(XILTIMER_tick_timer "None" CACHE STRING "This parameter is used to select specific timer for tick functionality")
    SET_PROPERTY(CACHE XILTIMER_tick_timer PROPERTY STRINGS "None")
endif()
option(XILTIMER_en_interval_timer "Enable Interval Timer" OFF)


if ("${XILTIMER_sleep_timer}" STREQUAL "${XILTIMER_tick_timer}")
    message(FATAL_ERROR "For sleep and tick functionality select different timers")
endif()

string(FIND "${CMAKE_C_FLAGS}" "-flto" has_flto)
list(LENGTH XILTIMER_sleep_timer sleep_timer_len)
list(LENGTH XILTIMER_tick_timer tick_timer_len)
if (NOT (${has_flto} EQUAL -1))
    set(XTIMER_NO_TICK_TIMER 1)
    set(XTIMER_IS_DEFAULT_TIMER 1)
    set(sleep_timer Default)
    set(tick_timer None)
elseif (${_len} GREATER 1)
    if ("${XILTIMER_sleep_timer}" STREQUAL "Default")
        set(XTIMER_IS_DEFAULT_TIMER 1)
    elseif((${sleep_timer_len} GREATER 1) AND
	   (${tick_timer_len} GREATER 1))
	list(GET TOTAL_TIMER_INSTANCES 0 sleep_timer)
        list(GET TOTAL_TIMER_INSTANCES 1 tick_timer)
    elseif(${sleep_timer_len} EQUAL 1)
        set(index 1)
        LIST_INDEX(${index} ${XILTIMER_sleep_timer} "${TOTAL_TIMER_INSTANCES}")
	if ("${XILTIMER_tick_timer}" STREQUAL "None;")
	    if (${index} EQUAL ${_len})
		MATH(EXPR index "${index}-2")
		list(GET TOTAL_TIMER_INSTANCES ${index} tick_timer)
	    else()
		list(GET TOTAL_TIMER_INSTANCES ${index} tick_timer)
	    endif()
	endif()
    endif()
    if ("${XILTIMER_tick_timer}" STREQUAL "None")
        set(XTIMER_NO_TICK_TIMER 1)
    endif()
elseif (${_len} EQUAL 1)
    if ((("${XILTIMER_tick_timer}" STREQUAL "None;") OR
	 ("${XILTIMER_tick_timer}" STREQUAL "None")) AND
	(${XILTIMER_en_interval_timer}))
        list(GET TOTAL_TIMER_INSTANCES 0 tick_timer)
	set(sleep_timer "Default")
        set(XTIMER_IS_DEFAULT_TIMER 1)
    elseif((${XILTIMER_en_interval_timer}) AND
           (XILTIMER_tick_timer IN_LIST TOTAL_TIMER_INSTANCES))
        set(sleep_timer "Default")
        set(XTIMER_IS_DEFAULT_TIMER 1)
    elseif((NOT ("${XILTIMER_sleep_timer}" STREQUAL "Default")) AND
	   (NOT (${XILTIMER_en_interval_timer})))
        set(XTIMER_NO_TICK_TIMER 1)
	set(tick_timer "None")
        list(GET TOTAL_TIMER_INSTANCES 0 sleep_timer)
    elseif("${XILTIMER_tick_timer}" STREQUAL "None;")
	set(tick_timer "None")
	set(XTIMER_NO_TICK_TIMER 1)
    endif()
    if ("${tick_timer}" STREQUAL "None")
	set(XTIMER_NO_TICK_TIMER 1)
    endif()
    if ("${sleep_timer}" STREQUAL "Default")
	   set(XTIMER_IS_DEFAULT_TIMER 1)
    endif()
else(${_len} EQUAL 0)
    set(XTIMER_NO_TICK_TIMER 1)
    set(XTIMER_IS_DEFAULT_TIMER 1)
endif()

if (DEFINED sleep_timer)
    set(XILTIMER_sleep_timer ${sleep_timer} CACHE STRING "This parameter is used to select specific timer for sleep functionality" FORCE)
endif()

if (DEFINED tick_timer)
    set(XILTIMER_tick_timer ${tick_timer} CACHE STRING "This parameter is used to select specific timer for tick functionality" FORCE)
endif()

set(COUNTS_PER_SECOND XSLEEPTIMER_FREQ)
if (("${XILTIMER_sleep_timer}" STREQUAL "Default") OR
    ("${XILTIMER_sleep_timer}" STREQUAL "Default;"))
    set(XPAR_INCLUDE "#include \"xparameters.h\"")

    if(("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "cortexa72")
	OR ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "cortexa53")
	OR ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "cortexa53-32")
	OR ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64"))
	set(XSLEEPTIMER_FREQ XPAR_CPU_TIMESTAMP_CLK_FREQ)
    endif()

    if(("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "cortexr5"))
	set(XSLEEPTIMER_FREQ XPAR_CPU_CORE_CLOCK_FREQ_HZ/64)
    endif()

    if(("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "cortexa9"))
        set(XSLEEPTIMER_FREQ XPAR_CPU_CORE_CLOCK_FREQ_HZ/2)
    endif()

    if(("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "microblaze") OR
       ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "microblazeel") OR
       ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "plm_microblaze") OR
       ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "pmu_microblaze"))
	set(XSLEEPTIMER_FREQ XPAR_CPU_CORE_CLOCK_FREQ_HZ/4)
    endif()

endif()

if (${CONFIG_AXI_TIMER})
    if(XILTIMER_sleep_timer IN_LIST TMRCTR_NUM_DRIVER_INSTANCES)
        set(XSLEEPTIMER_IS_AXITIMER " ")
        set(index 0)
        LIST_INDEX(${index} ${XILTIMER_sleep_timer} "${TMRCTR_NUM_DRIVER_INSTANCES}")
        list(GET TOTAL_TMRCTR_PROP_LIST ${index} reg)
        set(tmp ${${reg}})
        list(GET tmp -2 base_addr)
        list(GET tmp -1 freq)
        set(XSLEEPTIMER_BASEADDRESS ${base_addr})
	set(XSLEEPTIMER_FREQ ${freq})
        set(AXI_TIMER 1)
    endif()
    if(XILTIMER_tick_timer IN_LIST TMRCTR_NUM_DRIVER_INSTANCES)
        set(XTICKTIMER_IS_AXITIMER " ")
        set(index 0)
        LIST_INDEX(${index} ${XILTIMER_tick_timer} "${TMRCTR_NUM_DRIVER_INSTANCES}")
        list(GET TOTAL_TMRCTR_PROP_LIST ${index} reg)
        set(tmp ${${reg}})
        list(GET tmp -2 base_addr)
        set(XTICKTIMER_BASEADDRESS ${base_addr})
        set(AXI_TIMER 1)
    endif()
endif()

if(${CONFIG_TTCPS})
    if(XILTIMER_sleep_timer IN_LIST TTCPS_NUM_DRIVER_INSTANCES)
        set(XSLEEPTIMER_IS_TTCPS " ")
        set(index 0)
        LIST_INDEX(${index} ${XILTIMER_sleep_timer} "${TTCPS_NUM_DRIVER_INSTANCES}")
        list(GET TOTAL_TTCPS_PROP_LIST ${index} reg)
        set(tmp ${${reg}})
        list(GET tmp -2 base_addr)
        list(GET tmp -1 freq)
        set(XSLEEPTIMER_BASEADDRESS ${base_addr})
	set(XSLEEPTIMER_FREQ ${freq})
        set(TTCPS 1)
    endif()
    if(XILTIMER_tick_timer IN_LIST TTCPS_NUM_DRIVER_INSTANCES)
        set(XTICKTIMER_IS_TTCPS " ")
        set(index 0)
        LIST_INDEX(${index} ${XILTIMER_tick_timer} "${TTCPS_NUM_DRIVER_INSTANCES}")
        list(GET TOTAL_TTCPS_PROP_LIST ${index} reg)
        set(tmp ${${reg}})
        list(GET tmp -2 base_addr)
        set(XTICKTIMER_BASEADDRESS ${base_addr})
        set(TTCPS 1)
    endif()
endif()

if(${CONFIG_SCUTIMER})
    if(XILTIMER_sleep_timer IN_LIST SCUTIMER_NUM_DRIVER_INSTANCES)
        set(XSLEEPTIMER_IS_SCUTIMER " ")
        set(index 0)
        LIST_INDEX(${index} ${XILTIMER_sleep_timer} "${SCUTIMER_NUM_DRIVER_INSTANCES}")
        list(GET TOTAL_SCUTIMER_PROP_LIST ${index} reg)
        set(tmp ${${reg}})
        list(GET tmp -2 base_addr)
        list(GET tmp -1 freq)
        set(XSLEEPTIMER_BASEADDRESS ${base_addr})
	set(XSLEEPTIMER_FREQ ${freq})
        set(SCUTIMER 1)
    endif()
    if(XILTIMER_tick_timer IN_LIST SCUTIMER_NUM_DRIVER_INSTANCES)
        set(XTICKTIMER_IS_SCUTIMER " ")
        set(index 0)
        LIST_INDEX(${index} ${XILTIMER_tick_timer} "${SCUTIMER_NUM_DRIVER_INSTANCES}")
        list(GET TOTAL_SCUTIMER_PROP_LIST ${index} reg)
        set(tmp ${${reg}})
        list(GET tmp -2 base_addr)
        set(XTICKTIMER_BASEADDRESS ${base_addr})
        set(SCUTIMER 1)
    endif()
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/xtimer_config.h.in ${CMAKE_BINARY_DIR}/include/xtimer_config.h)
