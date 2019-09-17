import gym
from gym import spaces
from gym.utils import seeding
import grpc
from . import schedule_pb2
from . import schedule_pb2_grpc
import numpy as np
# from StringIO import StringIO
from io import StringIO


class HalideEnv(gym.Env):
    metadata = {'render.modes': ['ansi']}
    target = 'localhost:50051'

    def __init__(self, algorithm_id, input_image, max_stage_directive):
        assert algorithm_id is not None and input_image is not None and max_stage_directive > 0

        channel = grpc.insecure_channel(self.target)
        self.stub = schedule_pb2_grpc.ScheduleServiceStub(channel)

        request = schedule_pb2.ScheduleInitRequest(
            algorithm_id=algorithm_id,
            input_image=input_image,
            max_stage_directive=max_stage_directive
        )
        response = self.stub.init(request)

        assert response.max_stage > 0 and response.max_directive > 0
        assert response.schedule_map_range > 1 and response.init_time_sec > 0

        self.init_exec_time_sec = response.init_time_sec
        self.min_exec_time_sec = None
        self.best_exec_time_sec = response.init_time_sec
        self.max_stage = response.max_stage
        self.max_stage_directive = max_stage_directive
        self.action_count = dict()
        self.state_count = None
        self.np_random = None

        self.reward_scale = 100.0 / response.init_time_sec
        self.error_reward = -1.0
        self.timeout_error_reward = self.error_reward * response.max_stage
        self.noop_reward = 0.0

        obsv_low = 0
        obsv_high = 1000
        obsv_size = response.max_stage * max_stage_directive * (2 + response.max_param)

        self.action_space = spaces.Discrete(response.schedule_map_range)
        self.observation_space = spaces.Box(low=obsv_low, high=obsv_high, shape=(obsv_size,), dtype=np.int32)
        self.state = np.empty(self.observation_space.shape, dtype=self.observation_space.dtype)

        self.seed()
        self.reset()

    def seed(self, seed=None):
        self.np_random, seed = seeding.np_random(seed)
        return [seed]

    def step(self, action):
        if action == 0:
            return self._observation(self.noop_reward, True, False)

        if not self._unique(action):
            return self._observation(self.error_reward, False, True)

        request = self._request(action)
        response = self.stub.step(request)

        if response.exec_timeout and response.exec_error:
            return self._observation(self.timeout_error_reward, True, True)

        if response.exec_error:
            return self._observation(self.error_reward, False, True)

        self._state(request, response)
        return self._observation(self._reward(response), False, False)

    def reset(self):
        request = schedule_pb2.ScheduleResetRequest()
        response = self.stub.reset(request)

        self.min_exec_time_sec = self.init_exec_time_sec
        self.action_count.clear()
        self.state_count = 0
        self.state[:] = 0

        action = self.np_random.randint(self.action_space.n)
        request = self._request(action)
        response = self.stub.step(request)
        if not response.exec_error:
            self._state(request, response)
            self._reward(response)
        return np.array(self.state)

    def render(self, mode='ansi'):
        request = schedule_pb2.ScheduleRenderRequest()
        response = self.stub.render(request)

        out = StringIO()
        for line_content in response.schedule_str:
            out.write(line_content)
            out.write(' ')
        return out.getvalue()

    def close(self):
        request = schedule_pb2.ScheduleCloseRequest()
        response = self.stub.close(request)

    def _unique(self, action):
        count = self.action_count.get(action, 0) + 1
        return count == 1

    def _request(self, action):
        request = schedule_pb2.ScheduleStepRequest()
        request.op.map_code = action
        return request

    def _state(self, request, response):
        p = self.state_count * len(response.op.elem_id)
        for i, id in enumerate(response.op.elem_id):
            self.state[p + i] = id
        action = request.op.map_code
        self.action_count[action] = self.action_count.get(action, 0) + 1
        self.state_count += 1

    def _reward(self, response):
        if response.exec_time_sec < self.min_exec_time_sec:
            exec_diff = self.min_exec_time_sec - response.exec_time_sec
            self.min_exec_time_sec = response.exec_time_sec
            return exec_diff * self.reward_scale
        return 0.0

    def _observation(self, reward, done, error):
        ek = 'best_exec'
        sk = 'best_schedule'
        info = {ek: self.min_exec_time_sec, sk: 'n/a'}
        if not done and not error and self.min_exec_time_sec < self.best_exec_time_sec:
            self.best_exec_time_sec = self.min_exec_time_sec
            done = True
            info[sk] = self.render()
        if not done and not error and self.state_count >= (self.max_stage * self.max_stage_directive):
            done = True
        return np.array(self.state), reward, done, info
