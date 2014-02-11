#!/bin/bash

# debug message prefixes
errPrefix="\033[37;1;41m [${bs}] \033[0m"
succPrefix="\033[37;1;42m [${bs}] \033[0m"

# GLOBAL VARIABLES
CurrentProjectStatus_=0
CurrentProjectName_=""

# arg 1 - message text
function checkErr() {
	echo -en "${errPrefix} $1. Exiting..."
	echo ""
	exit 1
}

# arg 1 - message text
function succMessage() {
	echo -en "${succPrefix} $1"
	echo ""
}

# splits string by "/" symbols, check if archive alredy exists and download if not
# Untars given archive and enter that directory
# arg 1 - download link
function enterPacketDir() {
		# get packet
		linkArray=(${1//\// })
		linkArrayLen=${#linkArray[@]}
		archiveFile=${linkArray[linkArrayLen-1]}		
		if [ ! -f "$archiveFile" ]; then
			if [[ "$1" == http* ]] || [[ "$1" == ftp* ]]; then
				wget $1
			else
				cp $1 .
			fi
		fi

		# untar it
		suffix=".t"
		x="${archiveFile%%$suffix*}"
		[[ $x = $1 ]] && offset=-1 || offset=${#x}
		dirName=${archiveFile:0:offset}
		rm -rf "$dirName"
		tar -xvf "$archiveFile" > /dev/null
		cd $dirName || checkErr "failed to cd to ${dirName}"
}

# if config scripts werent generated, generate it
function conditionalAutogen() {
	if [ ! -f "configure" ]; then ./autogen.sh ; fi
}

# wrapper for autotools project building
# arg 1 - project name (just for printing statuses, no real influence on logic)
# arg 2 - project source dir
# arg 3 - project out dir
# arg 4 - path to output file (for check)
function ProjectBuildStart() {
	CurrentProjectName_=$1
	if [ ! -f $4 ]; then
		# no output file, we have to build
		CurrentProjectStatus_=0
		mkdir $3
		cd $3
		rm -rf "./*"
		# check if source are in the our repo, or link to packet?
		if [[ "$2" == *.gz ]] || [[ "$2" == *.bz2 ]] || [[ "$2" == *.tgz ]]; then
			#it's a link so get it and cd there
			enterPacketDir "$2"
		else
			# it is in repo, just cd
			cd "$2"			
			conditionalAutogen
			make clean
		fi
	else
		# output file exists, set flag and return
		CurrentProjectStatus_=1
	fi
}

# execute make && make install
# and print operation status
function ProjectBuildFinish() {
	if [ $CurrentProjectStatus_ -eq 0 ]; then
		make V=1 || checkErr "$CurrentProjectName_ make failed"
		make install || checkErr "$CurrentProjectName_ make install failed"
		succMessage "$CurrentProjectName_ have been built"
	else
		succMessage "$CurrentProjectName_ already built"
	fi
}