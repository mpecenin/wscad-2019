#include <cstdlib>
#include <fstream>
#include "Halide.h"
#include "halide_benchmark.h"
#include "halide_image_io.h"

using namespace std;
using namespace Halide;

// =============================================================

const string PATH = "../resource/";
const int SAMPLES = 10;
const int ITERS = 10;

// =============================================================

double blur(const int img, const int arch, const int sched) {
    string image_path = PATH;
    if (img == 1) {
        image_path += "alfaromeo_gray_4x.png";
    } else if (img == 2) {
        image_path += "alfaromeo_gray_16x.png";
    } else if (img == 3) {
        image_path += "alfaromeo_gray_64x.png";
    } else {
        throw invalid_argument("unknown input image");
    }
    Buffer<float> input = Tools::load_and_convert_image(image_path);
    Buffer<float> output = Buffer<float>(input.width(), input.height());
    Func in_b, blur_x, blur_y;
    Var x, y;

    in_b = BoundaryConditions::repeat_edge(input);
    blur_x(x, y) = (in_b(x - 1, y) + in_b(x, y) + in_b(x + 1, y)) / 3.f;
    blur_y(x, y) = (blur_x(x, y - 1) + blur_x(x, y) + blur_x(x, y + 1)) / 3.f;

    Target target = get_target_from_environment();
    double t;
    if (arch == 1) {
        if (sched == 1) {

            Var yi;
            blur_y.split(y, y, yi, 8).parallel(y).vectorize(x, 8);
            blur_x.store_at(blur_y, y).compute_at(blur_y, yi).vectorize(x, 8);

        } else if (sched == 2) {

            blur_y.estimate(x, 0, input.width()).estimate(y, 0, input.height());
            Pipeline p(blur_y);
            MachineParams machine_params(8, 16384, 40);
            //ofstream f("blur"+to_string(img)+".schedule");
            p.auto_schedule(target, machine_params);

        } else if (sched == 3) {

            //cout << "ppo cpu" << endl;
            Var xi, yi;
            blur_y.tile(x, y, xi, yi, 512, 16); blur_y.parallel(y); blur_x.compute_at(blur_y, yi); blur_x.compute_at(blur_y, x);

        } else if (sched == 4) {

            //blur_y.unroll(x, 4); blur_y.parallel(y); blur_y.bound(x, 0, 3848);
            blur_y.unroll(x, 4); blur_y.parallel(y); blur_y.bound(x, 0, input.width());

        } else if (sched == 5) {

            Var xi, yi;
            //blur_y.tile(x, y, xi, yi, 128, 8); blur_y.unroll(x, 2); blur_x.compute_at(blur_y, x); blur_y.parallel(y); blur_y.bound(y, 0, 2568); blur_y.unroll(x, 4);
            blur_y.tile(x, y, xi, yi, 128, 8); blur_y.unroll(x, 2); blur_x.compute_at(blur_y, x); blur_y.parallel(y); blur_y.bound(y, 0, input.height()); blur_y.unroll(x, 4);

        } else if (sched == 6) {

            Var xi, yi;
            blur_y.tile(x, y, xi, yi, 512, 16); blur_y.parallel(y); blur_x.compute_at(blur_y, x);

        } else {
            throw invalid_argument("unknown schedule type");
        }
        blur_y.compile_jit(target);
        t = Tools::benchmark(SAMPLES, ITERS, [&]{
            blur_y.realize(output, target);
        });
    } else if (arch == 2) {
        target.set_feature(Target::CUDACapability35);
        target.set_feature(Target::CUDA);
        if (sched == 1) {

            Var xi, yi;
            blur_y.gpu_tile(x, y, xi, yi, 16, 16);
            blur_x.compute_at(blur_y, x).gpu_threads(x, y);

        } else if (sched == 2) {

            throw invalid_argument("unsupported schedule type");

        } else if (sched == 3) {

            //cout << "ppo gpu" << endl;
            Var xi, yi;
            blur_y.gpu_tile(x, y, xi, yi, 64, 8); blur_y.unroll(yi, 4); blur_y.unroll(xi, 2);

        } else if (sched == 4) {

            Var xi, yi;
            blur_y.gpu_tile(x, y, xi, yi, 128, 8); blur_y.unroll(yi, 4); blur_y.unroll(xi, 2);

        } else if (sched == 5) {

            Var xi, yi;
            blur_y.gpu_tile(x, y, xi, yi, 64, 8); blur_y.unroll(xi, 2); blur_y.unroll(yi, 4);

        } else if (sched == 6) {

            Var xi, yi;
            blur_y.gpu_tile(x, y, xi, yi, 64, 16); blur_y.unroll(yi, 4); blur_y.unroll(xi, 2);

        } else {
            throw invalid_argument("unknown schedule type");
        }
        blur_y.compile_jit(target);
        t = Tools::benchmark(SAMPLES, ITERS, [&]{
            blur_y.realize(output, target);
            output.device_sync();
        });
    } else {
        throw invalid_argument("unknown target architecture");
    }
    //output.copy_to_host();
    //Tools::convert_and_save_image(output, "blur"+to_string(img)+to_string(arch)+to_string(sched)+".png");
    return t;
}

// =============================================================

Expr sum3x3(Func f, Var x, Var y) {
    return f(x-1, y-1) + f(x-1, y) + f(x-1, y+1) +
            f(x, y-1)   + f(x, y)   + f(x, y+1) +
            f(x+1, y-1) + f(x+1, y) + f(x+1, y+1);
}

double harris(const int img, const int arch, const int sched) {
    string image_path = PATH;
    if (img == 1) {
        image_path += "alfaromeo_rgb_4x.png";
    } else if (img == 2) {
        image_path += "alfaromeo_rgb_16x.png";
    } else if (img == 3) {
        image_path += "alfaromeo_rgb_64x.png";
    } else {
        throw invalid_argument("unknown input image");
    }
    Buffer<float> input = Tools::load_and_convert_image(image_path);
    Buffer<float> output = Buffer<float>(input.width(), input.height());
    Func in_b, gray, Iy, Ix, Ixx, Iyy, Ixy, Sxx, Syy, Sxy, det, trace, harris, shifted;
    Var x, y;

    in_b = BoundaryConditions::repeat_edge(input);
    gray(x, y) = 0.299f * in_b(x, y, 0) + 0.587f * in_b(x, y, 1) + 0.114f * in_b(x, y, 2);
    Iy(x, y) = gray(x-1, y-1)*(-1.0f/12) + gray(x-1, y+1)*(1.0f/12) +
            gray(x, y-1)*(-2.0f/12) + gray(x, y+1)*(2.0f/12) +
            gray(x+1, y-1)*(-1.0f/12) + gray(x+1, y+1)*(1.0f/12);
    Ix(x, y) = gray(x-1, y-1)*(-1.0f/12) + gray(x+1, y-1)*(1.0f/12) +
            gray(x-1, y)*(-2.0f/12) + gray(x+1, y)*(2.0f/12) +
            gray(x-1, y+1)*(-1.0f/12) + gray(x+1, y+1)*(1.0f/12);
    Ixx(x, y) = Ix(x, y) * Ix(x, y);
    Iyy(x, y) = Iy(x, y) * Iy(x, y);
    Ixy(x, y) = Ix(x, y) * Iy(x, y);
    Sxx(x, y) = sum3x3(Ixx, x, y);
    Syy(x, y) = sum3x3(Iyy, x, y);
    Sxy(x, y) = sum3x3(Ixy, x, y);
    det(x, y) = Sxx(x, y) * Syy(x, y) - Sxy(x, y) * Sxy(x, y);
    trace(x, y) = Sxx(x, y) + Syy(x, y);
    harris(x, y) = det(x, y) - 0.04f * trace(x, y) * trace(x, y);
    shifted(x, y) = harris(x + 2, y + 2);

    Target target = get_target_from_environment();
    double t;
    if (arch == 1) {
        if (sched == 1) {

            Var xi, yi;
            shifted.tile(x, y, xi, yi, 128, 128).vectorize(xi, 8).parallel(y);
            Ix.compute_at(shifted, x).vectorize(x, 8);
            Iy.compute_at(shifted, x).vectorize(x, 8);

        } else if (sched == 2) {

            shifted.estimate(x, 0, input.width()).estimate(y, 0, input.height());
            Pipeline p(shifted);
            MachineParams machine_params(8, 16384, 40);
            //ofstream f("harris"+to_string(img)+".schedule");
            p.auto_schedule(target, machine_params);

        } else if (sched == 3) {

            //cout << "ppo cpu" << endl;
            Var xi, yi;
            shifted.tile(x, y, xi, yi, 256, 64); shifted.parallel(y); gray.compute_at(shifted, x); Iy.compute_at(shifted, x); Ix.compute_at(shifted, x);

        } else if (sched == 4) {

            Var xi, yi;
            shifted.tile(x, y, xi, yi, 512, 8); shifted.parallel(y); gray.compute_at(shifted, x); Iy.compute_at(shifted, x); Ix.compute_at(shifted, x); gray.store_at(shifted, y); gray.unroll(x, 2); gray.unroll(x, 4);

        } else if (sched == 5) {

            Var xi, yi;
            shifted.tile(x, y, xi, yi, 512, 32); Ix.compute_at(shifted, x); shifted.parallel(y); gray.compute_at(shifted, x); Iy.compute_at(shifted, x); gray.unroll(x, 4);

        } else if (sched == 6) {

            Var xi, yi;
            shifted.tile(x, y, xi, yi, 512, 32); shifted.parallel(y); gray.compute_at(shifted, x); Iy.compute_at(shifted, x); gray.unroll(x, 4); Ix.compute_at(shifted, x);

        } else {
            throw invalid_argument("unknown schedule type");
        }
        shifted.compile_jit(target);
        t = Tools::benchmark(SAMPLES, ITERS, [&]{
            shifted.realize(output, target);
        });
    } else if (arch == 2) {
        target.set_feature(Target::CUDACapability35);
        target.set_feature(Target::CUDA);
        if (sched == 1) {

            Var xi, yi;
            shifted.gpu_tile(x, y, xi, yi, 14, 14);
            Ix.compute_at(shifted, x).gpu_threads(x, y);
            Iy.compute_at(shifted, x).gpu_threads(x, y);

        } else if (sched == 2) {

            throw invalid_argument("unsupported schedule type");

        } else if (sched == 3) {

            //cout << "ppo gpu" << endl;
            Var xi, yi;
            shifted.gpu_tile(x, y, xi, yi, 64, 16); shifted.unroll(yi, 2); shifted.unroll(yi, 4);

        } else if (sched == 4) {

            Var xi, yi;
            shifted.gpu_tile(x, y, xi, yi, 32, 16); shifted.unroll(yi, 4); shifted.unroll(yi, 2);

        } else if (sched == 5) {

            Var xi, yi;
            //shifted.compute_root(); shifted.gpu_tile(x, y, xi, yi, 8, 32); shifted.bound(y, 0, 2568); shifted.unroll(yi, 2); gray.compute_at(shifted, x); gray.gpu_threads(x, y); gray.unroll(y, 2); gray.unroll(x, 3);
            shifted.compute_root(); shifted.gpu_tile(x, y, xi, yi, 8, 32); shifted.bound(y, 0, input.height()); shifted.unroll(yi, 2); gray.compute_at(shifted, x); gray.gpu_threads(x, y); gray.unroll(y, 2); gray.unroll(x, 3);

        } else if (sched == 6) {

            Var xi, yi;
            shifted.gpu_tile(x, y, xi, yi, 32, 16); shifted.unroll(yi, 4);

        } else {
            throw invalid_argument("unknown schedule type");
        }
        shifted.compile_jit(target);
        t = Tools::benchmark(SAMPLES, ITERS, [&]{
            shifted.realize(output, target);
            output.device_sync();
        });
    } else {
        throw invalid_argument("unknown target architecture");
    }
    //output.copy_to_host();
    //Tools::convert_and_save_image(output, "harris"+to_string(img)+to_string(arch)+to_string(sched)+".png");
    return t;
}

// =============================================================

double interpolate(const int img, const int arch, const int sched) {
    string image_path = PATH;
    if (img == 1) {
        image_path += "alfaromeo_rgba_4x.png";
    } else if (img == 2) {
        image_path += "alfaromeo_rgba_16x.png";
    } else if (img == 3) {
        image_path += "alfaromeo_rgba_64x.png";
    } else {
        throw invalid_argument("unknown input image");
    }
    Buffer<float> input = Tools::load_and_convert_image(image_path);
    Buffer<float> output = Buffer<float>(input.width(), input.height(), 3);
    Func in_b, normalize;
    Var x, y, c;

    const int levels = 10;
    Func downsampled[levels];
    Func downx[levels];
    Func interpolated[levels];
    Func upsampled[levels];
    Func upsampledx[levels];

    in_b = BoundaryConditions::repeat_edge(input);
    downsampled[0](x, y, c) = in_b(x, y, c) * in_b(x, y, 3);
    for (int l = 1; l < levels; ++l) {
        Func prev = downsampled[l-1];
        if (l == 4) {
            Expr w = input.width()/(1 << l);
            Expr h = input.height()/(1 << l);
            prev = lambda(x, y, c, prev(clamp(x, 0, w), clamp(y, 0, h), c));
        }
        downx[l](x, y, c) = (prev(x*2-1, y, c) +
                2.0f * prev(x*2, y, c) +
                prev(x*2+1, y, c)) * 0.25f;
        downsampled[l](x, y, c) = (downx[l](x, y*2-1, c) +
                2.0f * downx[l](x, y*2, c) +
                downx[l](x, y*2+1, c)) * 0.25f;
    }
    interpolated[levels-1](x, y, c) = downsampled[levels-1](x, y, c);
    for (int l = levels-2; l >= 0; --l) {
        upsampledx[l](x, y, c) = (interpolated[l+1](x/2, y, c) +
                interpolated[l+1]((x+1)/2, y, c)) / 2.0f;
        upsampled[l](x, y, c) = (upsampledx[l](x, y/2, c) +
                upsampledx[l](x, (y+1)/2, c)) / 2.0f;
        interpolated[l](x, y, c) = downsampled[l](x, y, c) + (1.0f - downsampled[l](x, y, 3)) * upsampled[l](x, y, c);
    }
    normalize(x, y, c) = interpolated[0](x, y, c) / interpolated[0](x, y, 3);

    Target target = get_target_from_environment();
    double t;
    if (arch == 1) {
        if (sched == 1) {

            Var xi, yi;
            for (int l = 1; l < levels-1; ++l) {
                downsampled[l]
                    .compute_root()
                    .parallel(y, 8)
                    .vectorize(x, 4);
                interpolated[l]
                    .compute_root()
                    .parallel(y, 8)
                    .unroll(x, 2)
                    .unroll(y, 2)
                    .vectorize(x, 8);
            }
            normalize
                .reorder(c, x, y)
                .bound(c, 0, 3)
                .unroll(c)
                .tile(x, y, xi, yi, 2, 2)
                .unroll(xi)
                .unroll(yi)
                .parallel(y, 8)
                .vectorize(x, 8)
                .bound(x, 0, input.width())
                .bound(y, 0, input.height());

        } else if (sched == 2) {

            normalize.estimate(c, 0, 3).estimate(x, 0, input.width()).estimate(y, 0, input.height());
            Pipeline p(normalize);
            MachineParams machine_params(8, 16384, 40);
            //ofstream f("interpolate"+to_string(img)+".schedule");
            p.auto_schedule(target, machine_params);

        } else if (sched == 3) {

            // Initial schedule needed to compile (agent schedule_preprocessor)
            for (int l = 1; l < levels-1; ++l) {
                downsampled[l].compute_root();
                interpolated[l].compute_root();
            }
            //cout << "ppo cpu" << endl;
            Var xi, yi;
            downsampled[7].tile(x, y, xi, yi, 128, 8); normalize.parallel(y); downsampled[1].parallel(y); interpolated[1].parallel(c); interpolated[5].unroll(c, 3); normalize.vectorize(x, 16); downsampled[2].parallel(c); interpolated[2].parallel(y); downsampled[3].unroll(x, 2); interpolated[1].vectorize(x, 4);

        } else if (sched == 4) {

            // Initial schedule needed to compile (agent schedule_preprocessor)
            for (int l = 1; l < levels-1; ++l) {
                downsampled[l].compute_root();
                interpolated[l].compute_root();
            }
            normalize.parallel(y); interpolated[1].vectorize(x, 4); downsampled[1].parallel(c); downsampled[1].unroll(x, 4); interpolated[1].parallel(c); normalize.vectorize(x, 16); downsampled[2].parallel(y); downsampled[5].vectorize(x, 8); interpolated[2].unroll(x, 4);

        } else if (sched == 5) {

            // Initial schedule needed to compile (agent schedule_preprocessor)
            for (int l = 1; l < levels-1; ++l) {
                downsampled[l].compute_root();
                interpolated[l].compute_root();
            }
            Var xi, yi;
            //interpolated[2].tile(x, y, xi, yi, 256, 32); normalize.parallel(y); downsampled[1].parallel(y); normalize.bound(x, 0, 3848); interpolated[1].parallel(c); interpolated[1].vectorize(x, 8); normalize.unroll(x, 4); downsampled[2].parallel(y); normalize.unroll(x, 2); downsampled[2].vectorize(x, 16); interpolated[2].parallel(c); downsampled[2].parallel(c);
            interpolated[2].tile(x, y, xi, yi, 256, 32); normalize.parallel(y); downsampled[1].parallel(y); normalize.bound(x, 0, input.width()); interpolated[1].parallel(c); interpolated[1].vectorize(x, 8); normalize.unroll(x, 4); downsampled[2].parallel(y); normalize.unroll(x, 2); downsampled[2].vectorize(x, 16); interpolated[2].parallel(c); downsampled[2].parallel(c);

        } else if (sched == 6) {

            // Initial schedule needed to compile (agent schedule_preprocessor)
            for (int l = 1; l < levels-1; ++l) {
                downsampled[l].compute_root();
                interpolated[l].compute_root();
            }
            Var xi, yi;
            downsampled[6].tile(x, y, xi, yi, 16, 8); normalize.parallel(y); downsampled[1].parallel(c); normalize.vectorize(x, 16); interpolated[1].parallel(c); downsampled[2].tile(x, y, xi, yi, 64, 8); downsampled[1].unroll(x, 4); downsampled[2].parallel(y); interpolated[2].parallel(y);

        } else {
            throw invalid_argument("unknown schedule type");
        }
        normalize.compile_jit(target);
        t = Tools::benchmark(SAMPLES, ITERS, [&]{
            normalize.realize(output, target);
        });
    } else if (arch == 2) {
        target.set_feature(Target::CUDACapability35);
        target.set_feature(Target::CUDA);
        if (sched == 1) {

            // Original schedule (cpu stage, gpu memory limitation)
            /*
            Var yo, yi, xo, xi, ci;
            Func cpu_wrapper = normalize.in();
            cpu_wrapper
                .reorder(c, x, y)
                .bound(c, 0, 3)
                .tile(x, y, xo, yo, xi, yi, input.width()/4, input.height()/4)
                .vectorize(xi, 8);
            normalize
                .compute_at(cpu_wrapper, xo)
                .reorder(c, x, y)
                .gpu_tile(x, y, xi, yi, 16, 16)
                .unroll(c);
            for (int l = 1; l < levels; ++l) {
                int tile_size = 32 >> l;
                if (tile_size < 1) tile_size = 1;
                if (tile_size > 8) tile_size = 8;
                downsampled[l]
                    .compute_root()
                    .gpu_tile(x, y, c, xi, yi, ci, tile_size, tile_size, 4);
                if (l == 1 || l == 4) {
                    interpolated[l]
                        .compute_at(cpu_wrapper, xo)
                        .gpu_tile(x, y, c, xi, yi, ci, 8, 8, 4);
                } else {
                    int parent = l > 4 ? 4 : 1;
                    interpolated[l]
                        .compute_at(interpolated[parent], x)
                        .gpu_threads(x, y, c);
                }
            }
            normalize = cpu_wrapper;
            */

            // Adapted schedule (full gpu, better performance)
            Var xi, yi, ci;
            normalize
                .reorder(c, x, y)
                .bound(c, 0, 3)
                .gpu_tile(x, y, xi, yi, 16, 16)
                .unroll(c);
            for (int l = 1; l < levels; ++l) {
                int tile_size = 32 >> l;
                if (tile_size < 1) tile_size = 1;
                if (tile_size > 8) tile_size = 8;
                downsampled[l]
                    .compute_root()
                    .gpu_tile(x, y, c, xi, yi, ci, tile_size, tile_size, 4);
                if (l == 1 || l == 4) {
                    interpolated[l]
                        .compute_root()
                        .gpu_tile(x, y, c, xi, yi, ci, 8, 8, 4);
                } else {
                    int parent = l > 4 ? 4 : 1;
                    interpolated[l]
                        .compute_at(interpolated[parent], x)
                        .gpu_threads(x, y, c);
                }
            }

        } else if (sched == 2) {

            throw invalid_argument("unsupported schedule type");

        } else if (sched == 3) {

            // Initial schedule needed to compile (agent schedule_preprocessor)
            for (int l = 1; l < levels-1; ++l) {
                downsampled[l].compute_root();
                interpolated[l].compute_root();
            }
            //cout << "ppo gpu" << endl;
            Var xi, yi, ci;
            normalize.gpu_tile(x, y, c, xi, yi, ci, 8, 8, 3); downsampled[1].gpu_tile(x, y, c, xi, yi, ci, 8, 8, 4); interpolated[1].gpu_tile(x, y, c, xi, yi, ci, 16, 16, 4); downsampled[2].gpu_tile(x, y, c, xi, yi, ci, 8, 16, 4); downsampled[6].gpu_tile(x, y, c, xi, yi, ci, 16, 8, 4); interpolated[3].gpu_tile(x, y, c, xi, yi, ci, 8, 8, 4); downsampled[4].gpu_tile(x, y, c, xi, yi, ci, 16, 8, 4); downx[5].compute_at(downsampled[5], x); downsampled[8].gpu_tile(x, y, c, xi, yi, ci, 16, 16, 4); interpolated[1].unroll(yi, 2); interpolated[8].gpu_tile(x, y, c, xi, yi, ci, 16, 16, 4); interpolated[2].gpu_tile(x, y, c, xi, yi, ci, 32, 8, 4); interpolated[2].unroll(y, 3); downsampled[5].gpu_tile(x, y, c, xi, yi, ci, 8, 32, 4); downsampled[6].unroll(xi, 4); downsampled[2].unroll(ci, 4); downsampled[4].unroll(yi, 2); interpolated[2].unroll(xi, 3); downsampled[5].unroll(ci, 3); downsampled[3].gpu_tile(x, y, c, xi, yi, ci, 8, 16, 4);

        } else if (sched == 4) {

            // Initial schedule needed to compile (agent schedule_preprocessor)
            for (int l = 1; l < levels-1; ++l) {
                downsampled[l].compute_root();
                interpolated[l].compute_root();
            }
            Var xi, yi, ci;
            normalize.gpu_tile(x, y, c, xi, yi, ci, 32, 8, 3); downsampled[1].gpu_tile(x, y, c, xi, yi, ci, 8, 16, 4); interpolated[1].gpu_tile(x, y, c, xi, yi, ci, 8, 8, 4); downsampled[2].gpu_tile(x, y, c, xi, yi, ci, 8, 8, 4); interpolated[2].gpu_tile(x, y, c, xi, yi, ci, 16, 16, 4); normalize.unroll(ci, 3); downsampled[3].gpu_tile(x, y, c, xi, yi, ci, 8, 8, 4);

        } else if (sched == 5) {

            // Initial schedule needed to compile (agent schedule_preprocessor)
            for (int l = 1; l < levels-1; ++l) {
                downsampled[l].compute_root();
                interpolated[l].compute_root();
            }
            Var xi, yi, ci;
            normalize.gpu_tile(x, y, c, xi, yi, ci, 8, 32, 3); downsampled[1].gpu_tile(x, y, c, xi, yi, ci, 32, 8, 4); interpolated[1].gpu_tile(x, y, c, xi, yi, ci, 16, 8, 4); downsampled[2].gpu_tile(x, y, c, xi, yi, ci, 8, 8, 4); interpolated[2].gpu_tile(x, y, c, xi, yi, ci, 16, 16, 4); downsampled[3].gpu_tile(x, y, c, xi, yi, ci, 8, 32, 4); interpolated[5].unroll(x, 4); interpolated[3].unroll(x, 4); interpolated[6].unroll(y, 4); normalize.unroll(ci, 2); downsampled[1].unroll(xi, 2);

        } else if (sched == 6) {

            // Initial schedule needed to compile (agent schedule_preprocessor)
            for (int l = 1; l < levels-1; ++l) {
                downsampled[l].compute_root();
                interpolated[l].compute_root();
            }
            Var xi, yi, ci;
            normalize.gpu_tile(x, y, c, xi, yi, ci, 8, 16, 3); downsampled[1].gpu_tile(x, y, c, xi, yi, ci, 8, 8, 4); interpolated[1].gpu_tile(x, y, c, xi, yi, ci, 16, 8, 4); downsampled[2].gpu_tile(x, y, c, xi, yi, ci, 16, 8, 4); downsampled[3].gpu_tile(x, y, c, xi, yi, ci, 8, 8, 4); interpolated[6].unroll(c, 2); interpolated[5].unroll(y, 3); interpolated[2].gpu_tile(x, y, c, xi, yi, ci, 8, 16, 4); interpolated[8].unroll(c, 3); interpolated[3].gpu_tile(x, y, c, xi, yi, ci, 16, 16, 4); downsampled[4].gpu_tile(x, y, c, xi, yi, ci, 16, 8, 4);

        } else {
            throw invalid_argument("unknown schedule type");
        }
        normalize.compile_jit(target);
        t = Tools::benchmark(SAMPLES, ITERS, [&]{
            normalize.realize(output, target);
            output.device_sync();
        });
    } else {
        throw invalid_argument("unknown target architecture");
    }
    //output.copy_to_host();
    //Tools::convert_and_save_image(output, "interpolate"+to_string(img)+to_string(arch)+to_string(sched)+".png");
    return t;
}

// =============================================================

void run_test(const int algo, const int img, const int arch, const int sched) {
    double t;
    if (algo == 1) {
        t = blur(img, arch, sched);
    } else if (algo == 2) {
        t = harris(img, arch, sched);
    } else if (algo == 3) {
        t = interpolate(img, arch, sched);
    } else {
        throw invalid_argument("unknown algorithm");
    }
    printf("algo img arch sched exec_sec, %i, %i, %i, %i, %.9f\n", algo, img, arch, sched, t);
}

// =============================================================

int main(int argc, char** argv) {
    if (argc < 5) {
        printf("Usage: %s algo img arch sched\n", argv[0]);
        return 1;
    }
    run_test(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
    return 0;
}
