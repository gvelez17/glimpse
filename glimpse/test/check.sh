#!/bin/sh

#----------------------------------------------
# Check target suite
#----------------------------------------------
ERROR_LOG="../make_check.errors"

echo "Start check suite"
cd test
rm -f $ERROR_LOG

#----------------------------------------------
# Make simple index database for test.txt file
#----------------------------------------------
echo ""
echo "run test 1 [1 of 2]"
echo -en "index test... "
GLIMPSEINDEX_OUTPUT=`../bin/glimpseindex -b -H . test.txt`
GLIMPSEINDEX_EXIT_CODE="$?"

if [ "$GLIMPSEINDEX_EXIT_CODE" -ne "0" ]; then
  echo "Error occured when running command: ../bin/glimpseindex -b -H . test.txt" >>"$ERROR_LOG"
  echo "Exit code: ${GLIMPSEINDEX_EXIT_CODE}" >>"$ERROR_LOG"
  echo "Eror output: ${GLIMPSEINDEX_OUTPUT}" >>"$ERROR_LOG"
  echo "" >>"$ERROR_LOG"
fi

#----------------------------------------------
# Analyse indexing results
#----------------------------------------------
GREP_INDEX=`grep "glimpse" .glimpse_index`
if [ -f ".glimpse_index" -a -n "$GREP_INDEX" ]; then
	echo "ok"
else
	echo "fail"
fi

#----------------------------------------------
# Perform boolean search using generated db
#----------------------------------------------
echo ""
echo "run test 2 [2 of 2]"
echo -en "search test... "
GLIMPSE_OUTPUT=`../bin/glimpse -c -h -i -y -H . 'test;suite'`
GLIMPSE_EXIT_CODE="$?"

if [ "$GLIMPSE_EXIT_CODE" -ne "0" ]; then
  echo "Error occured when running command: ../bin/glimpse -c -h -i -y -H . 'test;suite'" >>"$ERROR_LOG"
  echo "Exit code: ${GLIMPSE_EXIT_CODE}" >>"$ERROR_LOG"
  echo "Eror output: ${GLIMPSE_OUTPUT}" >>"$ERROR_LOG"
  echo "" >>"$ERROR_LOG"
fi

#----------------------------------------------
# Analyse search results
#----------------------------------------------
if [ "$GLIMPSE_OUTPUT" = "1" ]; then
	echo "ok"
else
	echo "fail"
fi


#----------------------------------------------
# Clean up
#----------------------------------------------
rm -f .glimpse_*

cd ..
echo ""
echo "Done"