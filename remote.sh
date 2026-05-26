# @Author: Burgy Benjamin <minidfx>
# @Date:   2016-02-15T18:16:05+01:00
# @Email:  benjamin.burgy@gmail.com
# @Last modified by:   minidfx
# @Last modified time: 2016-03-22T20:17:46+01:00

#!/bin/bash

if [ -z $PROJECT_PATH ]; then
  echo "PROJECT_PATH environment variable is empty or not defined."
  exit 1
fi

if [ -z $PEBBLE_SDK_PATH ]; then
  echo "PEBBLE_SDK_PATH environment variable is empty or not defined."
  exit 2
fi

if [ -z $USERNAME ]; then
  echo "USERNAME environment variable is empty or not defined."
  exit 3
fi

if [ -z $SERVER_HOST ]; then
  echo "SERVER_HOST environment variable is empty or not defined."
  exit 4
fi

bash ./push.sh

if [ $? -gt 0 ]; then
    exit $?
fi

ssh $USERNAME@$SERVER_HOST "cd $PROJECT_PATH;$PEBBLE_SDK_PATH/pebble $@"
