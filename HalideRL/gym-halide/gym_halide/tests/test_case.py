import grpc
from gym_halide.envs import schedule_pb2
from gym_halide.envs import schedule_pb2_grpc
import numpy as np


def test_case1(algo_id, input_image, nu_step):
    channel = grpc.insecure_channel('localhost:50051')
    stub = schedule_pb2_grpc.ScheduleServiceStub(channel)

    print('--- init ---')
    init_rq = schedule_pb2.ScheduleInitRequest(
        algorithm_id=algo_id,
        input_image=input_image,
        max_stage_directive=6
    )
    init_rt = stub.init(init_rq)
    best_time = init_rt.init_time_sec
    is_best_time = False
    print('debug:', init_rq, '|', init_rt)

    for i in range(nu_step):
        print(i)
        print('--- step ---')
        step_rq = schedule_pb2.ScheduleStepRequest()
        step_rq.op.map_code = np.random.randint(init_rt.schedule_map_range)
        step_rt = stub.step(step_rq)
        print('debug:', step_rq, '|', step_rt)

        if (not step_rt.exec_error and step_rt.exec_time_sec < best_time):
            best_time = step_rt.exec_time_sec
            is_best_time = True

        if (is_best_time or step_rt.exec_error or step_rt.exec_timeout):
            print('--- render ---')
            print('debug: best_time', is_best_time, best_time,
                  'error', step_rt.exec_error,
                  'timeout', step_rt.exec_timeout)
            is_best_time = False
            render_rq = schedule_pb2.ScheduleRenderRequest()
            render_rt = stub.render(render_rq)
            for idx in range(len(render_rt.schedule_str)):
                print(render_rt.schedule_str[idx])

        if (step_rt.exec_error):
            print('--- reset ---')
            reset_rq = schedule_pb2.ScheduleResetRequest()
            reset_rt = stub.reset(reset_rq)
            best_time = init_rt.init_time_sec
            print('debug:', reset_rt)

    print('--- close ---')
    close_rq = schedule_pb2.ScheduleCloseRequest()
    close_rt = stub.close(close_rq)
    print('debug: best_time', best_time)


def test_case2(algo_id, input_image, nu_step, map_code):
    channel = grpc.insecure_channel('localhost:50051')
    stub = schedule_pb2_grpc.ScheduleServiceStub(channel)

    print('--- init ---')
    init_rq = schedule_pb2.ScheduleInitRequest(
        algorithm_id=algo_id,
        input_image=input_image,
        max_stage_directive=6
    )
    init_rt = stub.init(init_rq)
    best_time = init_rt.init_time_sec
    best_step_time = best_time
    is_best_time = False
    print('debug:', init_rq, '|', init_rt)

    for i in range(nu_step * len(map_code)):
        print(i)
        print('--- step ---')
        step_rq = schedule_pb2.ScheduleStepRequest()
        step_rq.op.map_code = map_code[i % len(map_code)]
        step_rt = stub.step(step_rq)
        print('debug:', step_rq, '|', step_rt)

        if (not step_rt.exec_error and step_rt.exec_time_sec < best_time):
            best_time = step_rt.exec_time_sec
            best_step_time = best_time if best_time < best_step_time else best_step_time
            is_best_time = True

        if (True or is_best_time or step_rt.exec_error or step_rt.exec_timeout):
            print('--- render ---')
            print('debug: best_time', is_best_time, best_time,
                  'error', step_rt.exec_error,
                  'timeout', step_rt.exec_timeout)
            is_best_time = False
            render_rq = schedule_pb2.ScheduleRenderRequest()
            render_rt = stub.render(render_rq)
            for idx in range(len(render_rt.schedule_str)):
                print(render_rt.schedule_str[idx])

        if (step_rt.exec_error or i % len(map_code) == len(map_code) - 1):
            print('--- reset ---')
            reset_rq = schedule_pb2.ScheduleResetRequest()
            reset_rt = stub.reset(reset_rq)
            best_time = init_rt.init_time_sec
            print('debug:', reset_rt)

    print('--- close ---')
    close_rq = schedule_pb2.ScheduleCloseRequest()
    close_rt = stub.close(close_rq)
    print('debug: best_step_time', best_step_time)


def test_case3(algo_id, input_image, nu_step, nu_test):
    for i in range(nu_test):
        test_case1(algo_id, input_image, nu_step)
        print('\n=== end test', i, '===\n\n')


def test_case4(algo_id, input_image, nu_step, map_code, nu_test):
    for i in range(nu_test):
        test_case2(algo_id, input_image, nu_step, map_code)
        print('\n=== end test', i, '===\n\n')


if __name__ == '__main__':
    # test_case1(10, 'alfaromeo_gray_64x.png', 10)
    # test_case1(20, 'alfaromeo_rgb_16x.png', 10)
    # test_case2(10, 'alfaromeo_gray_64x.png', 1, [26, 47, 49, 50, 54])  # Blur, original schedule
    # test_case2(20, 'alfaromeo_rgb_16x.png', 1, [22, 47, 49, 83, 90, 98, 105])  # Harris, original schedule
    pass
