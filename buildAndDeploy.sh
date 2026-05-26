# @Author: Burgy Benjamin <minidfx>
# @Date:   2016-02-16T20:11:38+01:00
# @Email:  benjamin.burgy@gmail.com
# @Last modified by:   minidfx
# @Last modified time: 2016-03-19T22:09:23+01:00

#!/bin/bash

bash ./remote.sh build

if [ $? -lt 1 ]; then
  bash ./deploy.sh
else
  exit $?
fi
