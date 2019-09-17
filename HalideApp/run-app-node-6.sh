#!/usr/bin/env bash

#SBATCH -p 1d
#SBATCH --nodelist node6-ib
#SBATCH --sockets-per-node 1
#SBATCH --cores-per-socket 8
#SBATCH --threads-per-core 1
#SBATCH --gres gpu:1
#SBATCH -o output/slurm-%j.out

module load compilers/gcc/6.4.0

if pgrep -x "app-halide" > /dev/null; then
  echo "Failed, App is already running"
  exit 1
fi

BASEDIR="$(pwd)"
APPDIR="$BASEDIR/app-halide"
LOGDIR="$BASEDIR/result/$(hostname)_app_$(date +%F-%H-%M-%S)"
LOGFILE="$LOGDIR/result.csv"

if [ -d $LOGDIR ]; then
  echo "Directory $LOGDIR already exists"
  exit 1
fi

mkdir -p $LOGDIR
echo "test, algo, img, arch, sched, exec_sec" > $LOGFILE
if [ $? -ne 0 ]; then
  echo "Can't create file $LOGFILE"
  exit 1
fi

export LD_LIBRARY_PATH="$BASEDIR/runtime:$LD_LIBRARY_PATH"
export PATH="$APPDIR:$HOME/.local/bin:$PATH"

export OMP_NUM_THREADS=8
export HL_NUM_THREADS=8
export HL_GPU_DEVICE=0

echo "Logging to $LOGFILE"
cd $APPDIR
let idx=1
for algo in {1..3}; do
for img in {1..3}; do
for arch in {1..2}; do
for sched in {1..6}; do
  if [ $arch -eq 2 ] && [ $sched -eq 2 ]; then
    continue
  fi
  echo -n "test $idx " | tee -a $LOGFILE
  echo ""
  numactl --membind=0 --cpunodebind=0 app-halide $algo $img $arch $sched >> $LOGFILE
  rc=$?
  if [ $rc -ne 0 ]; then
    echo "arg $algo $img $arch $sched end with rc $rc" | tee -a $LOGFILE
  fi
  let idx+=1
done
done
done
done

