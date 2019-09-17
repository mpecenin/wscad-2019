#!/usr/bin/env python3

from baselines.common.cmd_util import make_halide_env, halide_arg_parser
from baselines.common import tf_util as U
from baselines import logger
from gym_halide.envs import HalideEnv
import os
import datetime

def train(env_id, num_episodes, seed):
    from baselines.ppo1 import mlp_policy, pposgd_simple
    U.make_session(num_cpu=1).__enter__()
    def policy_fn(name, ob_space, ac_space):
        return mlp_policy.MlpPolicy(name=name, ob_space=ob_space, ac_space=ac_space,
            hid_size=64, num_hid_layers=2)
    env = make_halide_env(env_id, seed)
    pposgd_simple.learn(env, policy_fn,
            max_episodes=num_episodes,
            timesteps_per_actorbatch=256,
            clip_param=0.2, entcoeff=0.03,
            optim_epochs=4, optim_stepsize=2.5e-3, optim_batchsize=64,
            gamma=0.99, lam=0.95, schedule='linear',
        )
    env.close()

def main():
    args = halide_arg_parser().parse_args()
    HalideEnv.target = args.target
    log_dir = os.path.normpath(
        os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir, 'result',
        datetime.datetime.now().strftime(os.uname().nodename+"_"+args.env+"_%Y-%m-%d-%H-%M-%S-%f")))
    logger.configure(dir=log_dir, format_strs=['stdout', 'log', 'csv', 'tensorboard'])
    train(args.env, num_episodes=args.num_episodes, seed=args.seed)

if __name__ == '__main__':
    main()
