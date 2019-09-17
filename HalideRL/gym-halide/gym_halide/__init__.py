from gym.envs.registration import register

register(
    id='HalideBlur-v0',
    entry_point='gym_halide.envs:HalideEnv',
    kwargs={
        'algorithm_id': 10,
        'input_image': 'alfaromeo_gray_64x.png',
        'max_stage_directive': 6
    },
)

register(
    id='HalideBlur-v1',
    entry_point='gym_halide.envs:HalideEnv',
    kwargs={
        'algorithm_id': 10,
        'input_image': 'alfaromeo_gray_16x.png',
        'max_stage_directive': 6
    },
)

register(
    id='HalideBlur-v2',
    entry_point='gym_halide.envs:HalideEnv',
    kwargs={
        'algorithm_id': 11,
        'input_image': 'alfaromeo_gray_16x.png',
        'max_stage_directive': 8
    },
)

register(
    id='HalideBlur-v3',
    entry_point='gym_halide.envs:HalideEnv',
    kwargs={
        'algorithm_id': 11,
        'input_image': 'alfaromeo_gray_64x.png',
        'max_stage_directive': 8
    },
)

register(
    id='HalideBlur-v4',
    entry_point='gym_halide.envs:HalideEnv',
    kwargs={
        'algorithm_id': 12,
        'input_image': 'alfaromeo_gray_64x.png',
        'max_stage_directive': 8
    },
)

register(
    id='HalideHarris-v0',
    entry_point='gym_halide.envs:HalideEnv',
    kwargs={
        'algorithm_id': 20,
        'input_image': 'alfaromeo_rgb_16x.png',
        'max_stage_directive': 6
    },
)

register(
    id='HalideHarris-v1',
    entry_point='gym_halide.envs:HalideEnv',
    kwargs={
        'algorithm_id': 21,
        'input_image': 'alfaromeo_rgb_16x.png',
        'max_stage_directive': 6
    },
)

register(
    id='HalideHarris-v2',
    entry_point='gym_halide.envs:HalideEnv',
    kwargs={
        'algorithm_id': 22,
        'input_image': 'alfaromeo_rgb_16x.png',
        'max_stage_directive': 8
    },
)

register(
    id='HalideHarris-v3',
    entry_point='gym_halide.envs:HalideEnv',
    kwargs={
        'algorithm_id': 22,
        'input_image': 'alfaromeo_rgb_64x.png',
        'max_stage_directive': 8
    },
)

register(
    id='HalideHarris-v4',
    entry_point='gym_halide.envs:HalideEnv',
    kwargs={
        'algorithm_id': 23,
        'input_image': 'alfaromeo_rgb_64x.png',
        'max_stage_directive': 8
    },
)

register(
    id='HalideInterpolate-v0',
    entry_point='gym_halide.envs:HalideEnv',
    kwargs={
        'algorithm_id': 30,
        'input_image': 'alfaromeo_rgba_16x.png',
        'max_stage_directive': 12
    },
)

register(
    id='HalideInterpolate-v1',
    entry_point='gym_halide.envs:HalideEnv',
    kwargs={
        'algorithm_id': 31,
        'input_image': 'alfaromeo_rgba_16x.png',
        'max_stage_directive': 8
    },
)

register(
    id='HalideInterpolate-v2',
    entry_point='gym_halide.envs:HalideEnv',
    kwargs={
        'algorithm_id': 31,
        'input_image': 'alfaromeo_rgba_64x.png',
        'max_stage_directive': 8
    },
)

register(
    id='HalideInterpolate-v3',
    entry_point='gym_halide.envs:HalideEnv',
    kwargs={
        'algorithm_id': 32,
        'input_image': 'alfaromeo_rgba_64x.png',
        'max_stage_directive': 8
    },
)
