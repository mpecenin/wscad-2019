#include <cstdio>
#include <cstdlib>
#include <grpc++/grpc++.h>

using namespace std;
using grpc::Status;

// =============================================================

void test_case1(int algo_id, string input_image, int nu_step) {
    ScheduleServiceImpl service;
    ServerContext ctx;
    ScheduleInitRequest init_rq;
    ScheduleInitResponse init_rt;
    ScheduleStepRequest step_rq;
    ScheduleStepResponse step_rt;
    ScheduleResetRequest reset_rq;
    ScheduleResetResponse reset_rt;
    ScheduleRenderRequest render_rq;
    ScheduleRenderResponse render_rt;
    ScheduleCloseRequest close_rq;
    ScheduleCloseResponse close_rt;
    Status s;

    srand(time(NULL));

    cout << "--- init ---" << endl;
    init_rq.set_algorithm_id(algo_id);
    init_rq.set_input_image(input_image);
    init_rq.set_max_stage_directive(6);
    s = service.init(&ctx, &init_rq, &init_rt);
    if (!s.ok()) {
        cout << "grpc: " << s.error_message() << endl;
        abort();
    }
    double best_time = init_rt.init_time_sec();
    bool is_best_time = false;
    cout << "debug: " << init_rq.ShortDebugString() << " | " << init_rt.ShortDebugString() << endl;

    for (int i = 0; i < nu_step; i++) {
        cout << endl << i << endl;
        cout << "--- step ---" << endl;
        step_rq.Clear();
        step_rt.Clear();
        OperationRequest *op = step_rq.mutable_op();
        op->set_map_code(rand() % init_rt.schedule_map_range());
        s = service.step(&ctx, &step_rq, &step_rt);
        if (!s.ok()) {
            cout << "grpc: " << s.error_message() << endl;
            abort();
        }
        cout << "debug: " << step_rq.ShortDebugString() << " | " << step_rt.ShortDebugString() << endl;

        if (!step_rt.exec_error() && step_rt.exec_time_sec() < best_time) {
            best_time = step_rt.exec_time_sec();
            is_best_time = true;
        }

        if (is_best_time || step_rt.exec_error() || step_rt.exec_timeout()) {
            cout << "--- render ---" << endl;
            cout << "debug: best_time: " << is_best_time << " " << best_time << ", error: " << step_rt.exec_error() << ", timeout: " << step_rt.exec_timeout() << endl;
            is_best_time = false;
            render_rq.Clear();
            render_rt.Clear();
            s = service.render(&ctx, &render_rq, &render_rt);
            if (!s.ok()) {
                cout << "grpc: " << s.error_message() << endl;
                abort();
            }
            for (int idx = 0; idx < render_rt.schedule_str_size(); idx++)
                cout << render_rt.schedule_str(idx) << endl;
        }

        if (step_rt.exec_error()) {
            cout << "--- reset ---" << endl;
            reset_rq.Clear();
            reset_rt.Clear();
            s = service.reset(&ctx, &reset_rq, &reset_rt);
            if (!s.ok()) {
                cout << "grpc: " << s.error_message() << endl;
                abort();
            }
            best_time = init_rt.init_time_sec();
            cout << "debug: " << reset_rt.ShortDebugString() << endl;
        }
    }

    cout << endl;
    cout << "--- close ---" << endl;
    s = service.close(&ctx, &close_rq, &close_rt);
    if (!s.ok()) {
        cout << "grpc: " << s.error_message() << endl;
        abort();
    }
    cout << "debug: best_time: " << best_time << endl;
}

void test_case2(int algo_id, string input_image, int nu_step, vector<int> map_code) {
    ScheduleServiceImpl service;
    ServerContext ctx;
    ScheduleInitRequest init_rq;
    ScheduleInitResponse init_rt;
    ScheduleStepRequest step_rq;
    ScheduleStepResponse step_rt;
    ScheduleResetRequest reset_rq;
    ScheduleResetResponse reset_rt;
    ScheduleRenderRequest render_rq;
    ScheduleRenderResponse render_rt;
    ScheduleCloseRequest close_rq;
    ScheduleCloseResponse close_rt;
    Status s;

    cout << "--- init ---" << endl;
    init_rq.set_algorithm_id(algo_id);
    init_rq.set_input_image(input_image);
    init_rq.set_max_stage_directive(6);
    s = service.init(&ctx, &init_rq, &init_rt);
    if (!s.ok()) {
        cout << "grpc: " << s.error_message() << endl;
        abort();
    }
    double best_time = init_rt.init_time_sec();
    double best_step_time = best_time;
    bool is_best_time = false;
    cout << "debug: " << init_rq.ShortDebugString() << " | " << init_rt.ShortDebugString() << endl;

    for (int i = 0; i < nu_step * map_code.size(); i++) {
        cout << endl << i << endl;
        cout << "--- step ---" << endl;
        step_rq.Clear();
        step_rt.Clear();
        OperationRequest *op = step_rq.mutable_op();
        op->set_map_code(map_code[i % map_code.size()]);
        s = service.step(&ctx, &step_rq, &step_rt);
        if (!s.ok()) {
            cout << "grpc: " << s.error_message() << endl;
            abort();
        }
        cout << "debug: " << step_rq.ShortDebugString() << " | " << step_rt.ShortDebugString() << endl;

        if (!step_rt.exec_error() && step_rt.exec_time_sec() < best_time) {
            best_time = step_rt.exec_time_sec();
            best_step_time = best_time < best_step_time ? best_time : best_step_time;
            is_best_time = true;
        }

        if (true || is_best_time || step_rt.exec_error() || step_rt.exec_timeout()) {
            cout << "--- render ---" << endl;
            cout << "debug: best_time: " << is_best_time << " " << best_time << ", error: " << step_rt.exec_error() << ", timeout: " << step_rt.exec_timeout() << endl;
            is_best_time = false;
            render_rq.Clear();
            render_rt.Clear();
            s = service.render(&ctx, &render_rq, &render_rt);
            if (!s.ok()) {
                cout << "grpc: " << s.error_message() << endl;
                abort();
            }
            for (int idx = 0; idx < render_rt.schedule_str_size(); idx++)
                cout << render_rt.schedule_str(idx) << endl;
        }

        if (step_rt.exec_error() || i % map_code.size() == map_code.size() - 1) {
            cout << "--- reset ---" << endl;
            reset_rq.Clear();
            reset_rt.Clear();
            s = service.reset(&ctx, &reset_rq, &reset_rt);
            if (!s.ok()) {
                cout << "grpc: " << s.error_message() << endl;
                abort();
            }
            best_time = init_rt.init_time_sec();
            cout << "debug: " << reset_rt.ShortDebugString() << endl;
        }
    }

    cout << endl;
    cout << "--- close ---" << endl;
    s = service.close(&ctx, &close_rq, &close_rt);
    if (!s.ok()) {
        cout << "grpc: " << s.error_message() << endl;
        abort();
    }
    cout << "debug: best_step_time: " << best_step_time << endl;
}

void test_case3(int algo_id, string input_image, int nu_step, int nu_test) {
    for (int i = 0; i < nu_test; i++) {
        test_case1(algo_id, input_image, nu_step);
        cout << endl << "=== end test " << i << " ===" << endl << endl;
    }
}

void test_case4(int algo_id, string input_image, int nu_step, vector<int> map_code, int nu_test) {
    for (int i = 0; i < nu_test; i++) {
        test_case2(algo_id, input_image, nu_step, map_code);
        cout << endl << "=== end test " << i << " ===" << endl << endl;
    }
}

// =============================================================

//test_case1(10, "alfaromeo_gray_64x.png", 10);
//test_case2(10, "alfaromeo_gray_64x.png", 1, {26, 47, 49, 50, 54});
//test_case1(20, "alfaromeo_rgb_16x.png", 10);
//test_case2(20, "alfaromeo_rgb_16x.png", 1, {22, 47, 49, 83, 90, 98, 105});
//test_case1(30, "alfaromeo_rgba_16x.png", 10);
//test_case2(30, "alfaromeo_rgba_16x.png", 1, {2});
