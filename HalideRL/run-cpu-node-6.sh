#!/usr/bin/env bash

#SBATCH -p 7d
#SBATCH --nodelist node6-ib
#SBATCH --sockets-per-node 4
#SBATCH --cores-per-socket 8
#SBATCH --threads-per-core 1
#SBATCH -o output/slurm-%j.out

module load compilers/gcc/6.4.0
module load libraries/openmpi/3.0.0-gcc-6.4.0

if [ -z "$1" ]; then
  echo "Usage: $0 HalideEnv"
  exit 1
fi

if pgrep -x "grpc-halide" > /dev/null; then
  echo "Failed, Halide is already running"
  exit 1
fi

BASEDIR="$(pwd)"
HLDIR="$BASEDIR/grpc-halide"
AGDIR="$BASEDIR/baselines"

export LD_LIBRARY_PATH="$BASEDIR/runtime:$LD_LIBRARY_PATH"
export PATH="$HLDIR:$HOME/.local/bin:$PATH"
export PYTHONPATH="$AGDIR:$BASEDIR/gym-halide:$PYTHONPATH"

export OMP_NUM_THREADS=8
export HL_NUM_THREADS=8

cd $HLDIR
numactl --membind=0 --cpunodebind=0 grpc-halide "localhost:50051" &
numactl --membind=1 --cpunodebind=1 grpc-halide "localhost:50052" &
numactl --membind=2 --cpunodebind=2 grpc-halide "localhost:50053" &
numactl --membind=3 --cpunodebind=3 grpc-halide "localhost:50054" &

sleep 5

cd $AGDIR
numactl --membind=0 --cpunodebind=0 python3 baselines/ppo1/run_halide.py --env $1 --seed $(od -vAn -N4 -tu4 < /dev/urandom) --num-episodes 10000 --target "localhost:50051" &
numactl --membind=1 --cpunodebind=1 python3 baselines/ppo1/run_halide.py --env $1 --seed $(od -vAn -N4 -tu4 < /dev/urandom) --num-episodes 10000 --target "localhost:50052" &
numactl --membind=2 --cpunodebind=2 python3 baselines/ppo1/run_halide.py --env $1 --seed $(od -vAn -N4 -tu4 < /dev/urandom) --num-episodes 10000 --target "localhost:50053" &
numactl --membind=3 --cpunodebind=3 python3 baselines/ppo1/run_halide.py --env $1 --seed $(od -vAn -N4 -tu4 < /dev/urandom) --num-episodes 10000 --target "localhost:50054" &

jobs -l
wait %5 %6 %7 %8
kill %1 %2 %3 %4
wait %1 %2 %3 %4

