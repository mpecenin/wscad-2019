#!/usr/bin/env bash

LOGDIR="../result/$(hostname)_schedule_$(date +%F-%H-%M-%S)"
LOGFILE="$LOGDIR/result.csv"

if [ -d $LOGDIR ]; then
  echo "Directory $LOGDIR already exists"
  exit 1
fi

mkdir -p $LOGDIR
touch $LOGFILE
if [ $? -ne 0 ]; then
  echo "Can't create file $LOGFILE"
  exit 1
fi

# algo img arch sched
arg=()
arg[1]='1 1 1 2'
arg[2]='1 2 1 2'
arg[3]='1 3 1 2'
arg[4]='2 1 1 2'
arg[5]='2 2 1 2'
arg[6]='2 3 1 2'
arg[7]='3 1 1 2'
arg[8]='3 2 1 2'
arg[9]='3 3 1 2'

echo "Logging to $LOGFILE"
for idx in ${!arg[@]}; do
  echo -n "test $idx " | tee -a $LOGFILE
  echo ""
  ./dist/Debug/GNU-Linux/app-halide ${arg[$idx]} >> $LOGFILE
  rc=$?
  if [ $rc != 0 ]; then
    echo "arg ${arg[$idx]} end with rc $rc" | tee -a $LOGFILE
  fi
done

