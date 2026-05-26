# @Author: Burgy Benjamin <minidfx>
# @Date:   2016-02-15T18:16:05+01:00
# @Email:  benjamin.burgy@gmail.com
# @Last modified by:   minidfx
# @Last modified time: 2016-03-22T20:17:46+01:00

#!/bin/bash

if [ -z $SERVER_HOST ]; then
  echo "SERVER_HOST environment variable is empty or not defined."
  exit 4
fi

if [ -z $PROJECT_PATH ]; then
  echo "PROJECT_PATH environment variable is empty or not defined."
  exit 1
fi

rsync -zrh --delete --exclude ".atom-build.json" --exclude ".git/" --exclude "includes/" --exclude "*.sh" --exclude ".gitignore" --exclude .clang_complete --exclude .DS_Store --progress . $SERVER_HOST:$PROJECT_PATH
