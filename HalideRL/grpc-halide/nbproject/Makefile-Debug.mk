#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Environment
MKDIR=mkdir
CP=cp
GREP=grep
NM=nm
CCADMIN=CCadmin
RANLIB=ranlib
CC=gcc
CCC=g++
CXX=g++
FC=gfortran
AS=as

# Macros
CND_PLATFORM=GNU-Linux
CND_DLIB_EXT=so
CND_CONF=Debug
CND_DISTDIR=dist
CND_BUILDDIR=build

# Include project Makefile
include Makefile

# Object Directory
OBJECTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}

# Object Files
OBJECTFILES= \
	${OBJECTDIR}/main.o \
	${OBJECTDIR}/schedule.grpc.pb.o \
	${OBJECTDIR}/schedule.pb.o


# C Compiler Flags
CFLAGS=

# CC Compiler Flags
CCFLAGS=`libpng-config --cflags` -pthread 
CXXFLAGS=`libpng-config --cflags` -pthread 

# Fortran Compiler Flags
FFLAGS=

# Assembler Flags
ASFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=-L../halide/bin -Wl,-rpath,'../halide/bin' `pkg-config --libs protobuf` `pkg-config --libs grpc++` `pkg-config --libs grpc` -lHalide `pkg-config --libs libjpeg`  

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/grpc-halide

${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/grpc-halide: ${OBJECTFILES}
	${MKDIR} -p ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}
	${LINK.cc} -o ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/grpc-halide ${OBJECTFILES} ${LDLIBSOPTIONS} `libpng-config --ldflags` -lpthread -ldl -lrt -pthread

${OBJECTDIR}/main.o: main.cpp
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../halide/include -I../halide/tools `pkg-config --cflags protobuf` `pkg-config --cflags grpc++` `pkg-config --cflags grpc` `pkg-config --cflags libjpeg` -std=c++11  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main.o main.cpp

${OBJECTDIR}/schedule.grpc.pb.o: schedule.grpc.pb.cc
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../halide/include -I../halide/tools `pkg-config --cflags protobuf` `pkg-config --cflags grpc++` `pkg-config --cflags grpc` `pkg-config --cflags libjpeg` -std=c++11  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/schedule.grpc.pb.o schedule.grpc.pb.cc

${OBJECTDIR}/schedule.pb.o: schedule.pb.cc
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../halide/include -I../halide/tools `pkg-config --cflags protobuf` `pkg-config --cflags grpc++` `pkg-config --cflags grpc` `pkg-config --cflags libjpeg` -std=c++11  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/schedule.pb.o schedule.pb.cc

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
