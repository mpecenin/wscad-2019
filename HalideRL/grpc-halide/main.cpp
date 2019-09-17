#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <chrono>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <grpc++/grpc++.h>
#include "schedule.grpc.pb.h"
#include "schedule.pb.h"
#include "Halide.h"
#include "halide_benchmark.h"
#include "halide_image_io.h"

#define confirm(expr,msg) assert((expr)&&(msg))
//#define DEBUG 1

using namespace std;
using namespace Halide;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;

// =============================================================

class HalideStageMapper;
class HalideFuncMapper;

class HalideBaseMapper {
public:

    struct MapperOperation {
        int mapper;
        int* param;
        int stage_index;
        int directive_index;
        int* param_index;

        MapperOperation(int max_param) {
            param = new int[max_param];
            param_index = new int[max_param];
        }

        virtual ~MapperOperation() {
            delete[] param;
            delete[] param_index;
        }

        void clear(int max_param) {
            mapper = -1;
            stage_index = 0;
            directive_index = 0;
            for (int i = 0; i < max_param; i++) {
                param[i] = -1;
                param_index[i] = 0;
            }
        }

        bool empty() const {
            return mapper == -1;
        }
    };

private:

    /**
     * Up to XX elements each type (range size), but unlimited stages/funcs (last type).
     * Ordered list, zero means not used, so first range starts above.
     */
    enum class ElemType : int {
        boolean = 100,
        text = ElemType::boolean + 100,
        number = ElemType::text + 100,
        expr = ElemType::number + 100,
        rvar = ElemType::expr + 100,
        var = ElemType::rvar + 100,
        directive = ElemType::var + 100,
        stage = ElemType::directive + 100
    };

    vector<string> stage_indexer;
    vector<string> directive_indexer;
    vector<string> var_indexer;
    vector<string> rvar_indexer;
    vector<string> expr_indexer;
    vector<string> number_indexer;
    vector<string> text_indexer;
    vector<string> boolean_indexer;

    string stage_name_stack;
    vector<function<void()>> stage_directive_stack;

    struct MapperStorage {
        vector<vector<function<void()>>> mapper_directive;
        vector<vector<vector<int>>> mapper_index;
        vector<vector<int>> mapper_param_range;
        vector<int> mapper_range_accumulator;
    };

    MapperStorage schedule_mapper;
    int max_directive_param = 0;

    int get_index(const ElemType &elem_type, const string &elem_name, vector<string> &indexer) {
        for (int i = 0; i < indexer.size(); i++) {
            if (elem_name == indexer[i])
                return i + (int) elem_type;
        }
        indexer.push_back(elem_name);
        return (indexer.size() - 1) + (int) elem_type;
    }

    vector<int> stage_index(const string &stage_name) {
        return {get_index(ElemType::stage, stage_name, stage_indexer)};
    }

    vector<int> directive_index(const string &directive_name) {
        return {get_index(ElemType::directive, directive_name, directive_indexer)};
    }

    vector<int> elem_index(const vector<Func> &elem) {
        vector<int> index;
        index.reserve(elem.size());
        for (int i = 0; i < elem.size(); i++) {
            index.push_back(get_index(ElemType::stage, elem[i].name(), stage_indexer));
        }
        return index;
    }

    vector<int> elem_index(const vector<Stage> &elem) {
        vector<int> index;
        index.reserve(elem.size());
        for (int i = 0; i < elem.size(); i++) {
            index.push_back(get_index(ElemType::stage, elem[i].name(), stage_indexer));
        }
        return index;
    }

    vector<int> elem_index(const vector<Var> &elem) {
        vector<int> index;
        index.reserve(elem.size());
        for (int i = 0; i < elem.size(); i++) {
            index.push_back(get_index(ElemType::var, elem[i].name(), var_indexer));
        }
        return index;
    }

    vector<int> elem_index(const vector<RVar> &elem) {
        vector<int> index;
        index.reserve(elem.size());
        for (int i = 0; i < elem.size(); i++) {
            index.push_back(get_index(ElemType::rvar, elem[i].name(), rvar_indexer));
        }
        return index;
    }

    vector<int> elem_index(const vector<VarOrRVar> &elem) {
        vector<int> index;
        index.reserve(elem.size());
        for (int i = 0; i < elem.size(); i++) {
            if (elem[i].is_rvar)
                index.push_back(get_index(ElemType::rvar, elem[i].name(), rvar_indexer));
            else
                index.push_back(get_index(ElemType::var, elem[i].name(), var_indexer));
        }
        return index;
    }

    vector<int> elem_index(const vector<Expr> &elem) {
        vector<int> index;
        index.reserve(elem.size());
        stringstream ss;
        for (int i = 0; i < elem.size(); i++) {
            ss.str("");
            ss << elem[i];
            index.push_back(get_index(ElemType::expr, ss.str(), expr_indexer));
        }
        return index;
    }

    vector<int> elem_index(const vector<int> &elem) {
        vector<int> index;
        index.reserve(elem.size());
        for (int i = 0; i < elem.size(); i++) {
            index.push_back(get_index(ElemType::number, to_string(elem[i]), number_indexer));
        }
        return index;
    }

    vector<int> elem_index(const vector<float> &elem) {
        vector<int> index;
        index.reserve(elem.size());
        for (int i = 0; i < elem.size(); i++) {
            index.push_back(get_index(ElemType::number, to_string(elem[i]), number_indexer));
        }
        return index;
    }

    vector<int> elem_index(const vector<string> &elem) {
        vector<int> index;
        index.reserve(elem.size());
        for (int i = 0; i < elem.size(); i++) {
            index.push_back(get_index(ElemType::text, elem[i], text_indexer));
        }
        return index;
    }

    vector<int> elem_index(const vector<bool> &elem) {
        vector<int> index;
        index.reserve(elem.size());
        for (int i = 0; i < elem.size(); i++) {
            index.push_back(get_index(ElemType::boolean, elem[i] ? "true" : "false", boolean_indexer));
        }
        return index;
    }

    template<typename T>
    vector<int> elem_index(const T &elem) {
        vector<T> aux = {elem};
        return elem_index(aux);
    }

    int encode_range(const vector<int> &directive_param_range) {
        int range = 1;
        for (int param_range : directive_param_range) {
            range *= param_range;
        }
        return range;
    }

    void decode_param(const int mapper, const int param_code, MapperOperation &op) {
        MapperStorage &sm = schedule_mapper;
        vector<int> &param_range = sm.mapper_param_range[mapper];
        int value = param_code;
        for (int i = param_range.size() - 1; i >= 0; i--) {
            int base = param_range[i];

            int param = value % base;
            value = value / base;

            op.param[i] = param;
            op.param_index[i] = sm.mapper_index[mapper][i + 2][param];
        }
        for (int i = param_range.size(); i < max_directive_param; i++) {
            op.param[i] = -1;
            op.param_index[i] = 0;
        }
    }

    void decode_mapper(const int map_code, MapperOperation &op) {
        MapperStorage &sm = schedule_mapper;
        vector<int>::iterator pos = lower_bound(sm.mapper_range_accumulator.begin(), sm.mapper_range_accumulator.end(), map_code + 1);
        int mapper = pos - sm.mapper_range_accumulator.begin();

        op.mapper = mapper;
        op.stage_index = sm.mapper_index[mapper][0][0];
        op.directive_index = sm.mapper_index[mapper][1][0];

        int previous_range = mapper > 0 ? sm.mapper_range_accumulator[mapper - 1] : 0;
        decode_param(mapper, map_code - previous_range, op);
    }

protected:

    Func df = Func(Expr(0)); // Dynamically created func
    Stage ds = (Stage)df; // Dynamically created stage
    Func *cf = nullptr; // Current exec func
    Stage *cs = nullptr; // Current exec stage
    int *cp = nullptr; // Current exec mapper param

    template<typename... Types>
    vector<vector<int>> make_index(const string &directive_name, const Types&... directive_param_options) {
        return {stage_index(stage_name_stack), directive_index(directive_name), elem_index(directive_param_options)...};
    }

    void init_mapper() {
        stage_name_stack.clear();
        stage_directive_stack.clear();
    }

    /**
     * Previous auxiliary mappers to access one specific/dynamic stage/func.
     */
    void stack_mapper(
            const string &stage_name,
            const function<void()> &stage_directive_lambda) {
        stage_name_stack += stage_name;
        stage_directive_stack.push_back(stage_directive_lambda);
    }

    /**
     * Each mapper is related to only one stage/func and directive.
     */
    void add_mapper(
            const function<void()> &directive_lambda,
            const vector<vector<int>> &elem_index) {

        MapperStorage &sm = schedule_mapper;

        sm.mapper_directive.push_back(stage_directive_stack);
        sm.mapper_directive.back().push_back(directive_lambda);

        sm.mapper_index.push_back(elem_index);

        vector<int> param_elem_range;
        param_elem_range.reserve(elem_index.size() - 2);
        for (int i = 2; i < elem_index.size(); i++) {
            param_elem_range.push_back(elem_index[i].size());
        }
        sm.mapper_param_range.push_back(param_elem_range);

        if (sm.mapper_range_accumulator.size() > 0)
            sm.mapper_range_accumulator.push_back(sm.mapper_range_accumulator.back() + encode_range(param_elem_range));
        else
            sm.mapper_range_accumulator.push_back(encode_range(param_elem_range));

        if (param_elem_range.size() > max_directive_param)
            max_directive_param = param_elem_range.size();
    }

public:

    int max_stage() {
        return stage_indexer.size();
    }

    int max_directive() {
        return directive_indexer.size();
    }

    int max_param() {
        return max_directive_param;
    }

    int map_range() {
        MapperStorage &sm = schedule_mapper;
        return sm.mapper_range_accumulator.back() + 1; // With no-op
    }

    void decode(const int map_code, MapperOperation &op) {
        if (map_code < 0 || map_code >= map_range())
            throw domain_error("Map code " + to_string(map_code) + " outside schedule range");
        if (map_code == 0)
            op.clear(max_param()); // No-op
        else
            decode_mapper(map_code - 1, op); // Without no-op
    }

    void apply(const MapperOperation &op) {
        if (op.empty()) // No-op
            return;
        MapperStorage &sm = schedule_mapper;
        vector<function<void()>> &md = sm.mapper_directive[op.mapper];
        cp = op.param;
        for (int i = 0; i < md.size(); i++) {
            md[i]();
        }
    }

    void render(const MapperOperation &op, string &s) {
        if (op.empty()) // No-op
            return;
        s += stage_indexer[op.stage_index - (int) ElemType::stage];
        s += ".";
        s += directive_indexer[op.directive_index - (int) ElemType::directive];
        s += "(";
        for (int i = 0; i < max_directive_param; i++) {
            int elem = op.param_index[i];
            if (elem == 0)
                break;
            if (i > 0)
                s += ", ";
            if (elem >= (int) ElemType::stage)
                s += stage_indexer[elem - (int) ElemType::stage];
            else if (elem >= (int) ElemType::var)
                s += var_indexer[elem - (int) ElemType::var];
            else if (elem >= (int) ElemType::rvar)
                s += rvar_indexer[elem - (int) ElemType::rvar];
            else if (elem >= (int) ElemType::expr)
                s += expr_indexer[elem - (int) ElemType::expr];
            else if (elem >= (int) ElemType::number)
                s += number_indexer[elem - (int) ElemType::number];
            else if (elem >= (int) ElemType::text)
                s += text_indexer[elem - (int) ElemType::text];
            else if (elem >= (int) ElemType::boolean)
                s += boolean_indexer[elem - (int) ElemType::boolean];
        }
        s += ");";
    }

};

class HalideStageMapper : protected HalideBaseMapper {
public:

    HalideFuncMapper &rfactor(RVar r, Var v) {
        // Can be used alone.
        add_mapper([=]{
            df = cs->rfactor(r, v);
            cf = &df;
        },
        make_index(__func__, r, v));
        // Or can be used to schedule your own sub func.
        stringstream ss;
        ss << "." << __func__ << "(" << r.name() << ", " << v.name() << ")";
        stack_mapper(ss.str(), [=]{
            df = cs->rfactor(r, v);
            cf = &df;
        });
        return (HalideFuncMapper&) (*this);
    }

    HalideStageMapper &compute_with(const vector<Stage> &s, const vector<VarOrRVar> &var) {
        add_mapper([=]{
            cs->compute_with(s[cp[0]], var[cp[1]]);
        },
        make_index(__func__, s, var));
        return *this;
    }

    HalideStageMapper &split(const vector<VarOrRVar> &old,
            const vector<VarOrRVar> &outer, const vector<VarOrRVar> &inner, const vector<Expr> &factor) {
        add_mapper([=]{
            cs->split(old[cp[0]], outer[cp[1]], inner[cp[2]], factor[cp[3]]);
        },
        make_index(__func__, old, outer, inner, factor));
        return *this;
    }

    HalideStageMapper &split(const vector<VarOrRVar> &old_outer, const vector<VarOrRVar> &inner, const vector<Expr> &factor) {
        add_mapper([=]{
            cs->split(old_outer[cp[0]], old_outer[cp[0]], inner[cp[1]], factor[cp[2]]);
        },
        make_index(__func__, old_outer, inner, factor));
        return *this;
    }

    HalideStageMapper &fuse(const vector<VarOrRVar> &inner, const vector<VarOrRVar> &outer, const vector<VarOrRVar> &fused) {
        add_mapper([=]{
            cs->fuse(inner[cp[0]], outer[cp[1]], fused[cp[2]]);
        },
        make_index(__func__, inner, outer, fused));
        return *this;
    }

    HalideStageMapper &serial(const vector<VarOrRVar> &var) {
        add_mapper([=]{
            cs->serial(var[cp[0]]);
        },
        make_index(__func__, var));
        return *this;
    }

    HalideStageMapper &parallel(const vector<VarOrRVar> &var) {
        add_mapper([=]{
            cs->parallel(var[cp[0]]);
        },
        make_index(__func__, var));
        return *this;
    }

    HalideStageMapper &parallel(const vector<VarOrRVar> &var, const vector<Expr> &task_size) {
        add_mapper([=]{
            cs->parallel(var[cp[0]], task_size[cp[1]]);
        },
        make_index(__func__, var, task_size));
        return *this;
    }

    HalideStageMapper &vectorize(const vector<VarOrRVar> &var) {
        add_mapper([=]{
            cs->vectorize(var[cp[0]]);
        },
        make_index(__func__, var));
        return *this;
    }

    HalideStageMapper &vectorize(const vector<VarOrRVar> &var, const vector<Expr> &factor) {
        add_mapper([=]{
            cs->vectorize(var[cp[0]], factor[cp[1]]);
        },
        make_index(__func__, var, factor));
        return *this;
    }

    HalideStageMapper &unroll(const vector<VarOrRVar> &var) {
        add_mapper([=]{
            cs->unroll(var[cp[0]]);
        },
        make_index(__func__, var));
        return *this;
    }

    HalideStageMapper &unroll(const vector<VarOrRVar> &var, const vector<Expr> &factor) {
        add_mapper([=]{
            cs->unroll(var[cp[0]], factor[cp[1]]);
        },
        make_index(__func__, var, factor));
        return *this;
    }

    HalideStageMapper &tile(const vector<VarOrRVar> &x, const vector<VarOrRVar> &y,
            const vector<VarOrRVar> &xo, const vector<VarOrRVar> &yo,
            const vector<VarOrRVar> &xi, const vector<VarOrRVar> &yi,
            const vector<Expr> &xfactor, const vector<Expr> &yfactor) {
        add_mapper([=]{
            cs->tile(x[cp[0]], y[cp[1]], xo[cp[2]], yo[cp[3]], xi[cp[4]], yi[cp[5]], xfactor[cp[6]], yfactor[cp[7]]);
        },
        make_index(__func__, x, y, xo, yo, xi, yi, xfactor, yfactor));
        return *this;
    }

    HalideStageMapper &tile(const vector<VarOrRVar> &x, const vector<VarOrRVar> &y,
            const vector<VarOrRVar> &xi, const vector<VarOrRVar> &yi,
            const vector<Expr> &xfactor, const vector<Expr> &yfactor) {
        add_mapper([=]{
            cs->tile(x[cp[0]], y[cp[1]], xi[cp[2]], yi[cp[3]], xfactor[cp[4]], yfactor[cp[5]]);
        },
        make_index(__func__, x, y, xi, yi, xfactor, yfactor));
        return *this;
    }

    template<typename... Args>
    HalideStageMapper &reorder(const vector<VarOrRVar> &x, const vector<VarOrRVar> &y, const Args&... args) {
        add_mapper([=]{
            const int nargs = 2 + tuple_size<tuple<Args...>>::value;
            int* ap = cp + nargs;
            cs->reorder(x[cp[0]], y[cp[1]], args[*--ap]...);
        },
        make_index(__func__, x, y, args...));
        return *this;
    }

    HalideStageMapper &specialize(Expr condition) {
        // Can be used alone.
        add_mapper([=]{
            ds = cs->specialize(condition);
            cs = &ds;
        },
        make_index(__func__, condition));
        // Or can be used to schedule your own sub stage.
        stringstream ss;
        ss << "." << __func__ << "(" << condition << ")";
        stack_mapper(ss.str(), [=]{
            ds = cs->specialize(condition);
            cs = &ds;
        });
        return *this;
    }

    HalideStageMapper &specialize_fail(const std::string &message) {
        add_mapper([=]{
            cs->specialize_fail(message);
        },
        make_index(__func__, message));
        return *this;
    }

    HalideStageMapper &gpu_threads(const vector<VarOrRVar> &thread_x) {
        add_mapper([=]{
            cs->gpu_threads(thread_x[cp[0]]);
        },
        make_index(__func__, thread_x));
        return *this;
    }

    HalideStageMapper &gpu_threads(const vector<VarOrRVar> &thread_x, const vector<VarOrRVar> &thread_y) {
        add_mapper([=]{
            cs->gpu_threads(thread_x[cp[0]], thread_y[cp[1]]);
        },
        make_index(__func__, thread_x, thread_y));
        return *this;
    }

    HalideStageMapper &gpu_threads(const vector<VarOrRVar> &thread_x, const vector<VarOrRVar> &thread_y, const vector<VarOrRVar> &thread_z) {
        add_mapper([=]{
            cs->gpu_threads(thread_x[cp[0]], thread_y[cp[1]], thread_z[cp[2]]);
        },
        make_index(__func__, thread_x, thread_y, thread_z));
        return *this;
    }

    HalideStageMapper &gpu_blocks(const vector<VarOrRVar> &block_x) {
        add_mapper([=]{
            cs->gpu_blocks(block_x[cp[0]]);
        },
        make_index(__func__, block_x));
        return *this;
    }

    HalideStageMapper &gpu_blocks(const vector<VarOrRVar> &block_x, const vector<VarOrRVar> &block_y) {
        add_mapper([=]{
            cs->gpu_blocks(block_x[cp[0]], block_y[cp[1]]);
        },
        make_index(__func__, block_x, block_y));
        return *this;
    }

    HalideStageMapper &gpu_blocks(const vector<VarOrRVar> &block_x, const vector<VarOrRVar> &block_y, const vector<VarOrRVar> &block_z) {
        add_mapper([=]{
            cs->gpu_blocks(block_x[cp[0]], block_y[cp[1]], block_z[cp[2]]);
        },
        make_index(__func__, block_x, block_y, block_z));
        return *this;
    }

    HalideStageMapper &gpu(const vector<VarOrRVar> &block_x, const vector<VarOrRVar> &thread_x) {
        add_mapper([=]{
            cs->gpu(block_x[cp[0]], thread_x[cp[1]]);
        },
        make_index(__func__, block_x, thread_x));
        return *this;
    }

    HalideStageMapper &gpu(const vector<VarOrRVar> &block_x, const vector<VarOrRVar> &block_y,
            const vector<VarOrRVar> &thread_x, const vector<VarOrRVar> &thread_y) {
        add_mapper([=]{
            cs->gpu(block_x[cp[0]], block_y[cp[1]], thread_x[cp[2]], thread_y[cp[3]]);
        },
        make_index(__func__, block_x, block_y, thread_x, thread_y));
        return *this;
    }

    HalideStageMapper &gpu(const vector<VarOrRVar> &block_x, const vector<VarOrRVar> &block_y, const vector<VarOrRVar> &block_z,
            const vector<VarOrRVar> &thread_x, const vector<VarOrRVar> &thread_y, const vector<VarOrRVar> &thread_z) {
        add_mapper([=]{
            cs->gpu(block_x[cp[0]], block_y[cp[1]], block_z[cp[2]], thread_x[cp[3]], thread_y[cp[4]], thread_z[cp[5]]);
        },
        make_index(__func__, block_x, block_y, block_z, thread_x, thread_y, thread_z));
        return *this;
    }

    HalideStageMapper &gpu_tile(const vector<VarOrRVar> &x,
            const vector<VarOrRVar> &bx, const vector<VarOrRVar> &tx, const vector<Expr> &x_size) {
        add_mapper([=]{
            cs->gpu_tile(x[cp[0]], bx[cp[1]], tx[cp[2]], x_size[cp[3]]);
        },
        make_index(__func__, x, bx, tx, x_size));
        return *this;
    }

    HalideStageMapper &gpu_tile(const vector<VarOrRVar> &x, const vector<VarOrRVar> &tx, const vector<Expr> &x_size) {
        add_mapper([=]{
            cs->gpu_tile(x[cp[0]], tx[cp[1]], x_size[cp[2]]);
        },
        make_index(__func__, x, tx, x_size));
        return *this;
    }

    HalideStageMapper &gpu_tile(const vector<VarOrRVar> &x, const vector<VarOrRVar> &y,
            const vector<VarOrRVar> &bx, const vector<VarOrRVar> &by,
            const vector<VarOrRVar> &tx, const vector<VarOrRVar> &ty,
            const vector<Expr> &x_size, const vector<Expr> &y_size) {
        add_mapper([=]{
            cs->gpu_tile(x[cp[0]], y[cp[1]], bx[cp[2]], by[cp[3]], tx[cp[4]], ty[cp[5]], x_size[cp[6]], y_size[cp[7]]);
        },
        make_index(__func__, x, y, bx, by, tx, ty, x_size, y_size));
        return *this;
    }

    HalideStageMapper &gpu_tile(const vector<VarOrRVar> &x, const vector<VarOrRVar> &y,
            const vector<VarOrRVar> &tx, const vector<VarOrRVar> &ty,
            const vector<Expr> &x_size, const vector<Expr> &y_size) {
        add_mapper([=]{
            cs->gpu_tile(x[cp[0]], y[cp[1]], tx[cp[2]], ty[cp[3]], x_size[cp[4]], y_size[cp[5]]);
        },
        make_index(__func__, x, y, tx, ty, x_size, y_size));
        return *this;
    }

    HalideStageMapper &gpu_tile(const vector<VarOrRVar> &x, const vector<VarOrRVar> &y, const vector<VarOrRVar> &z,
            const vector<VarOrRVar> &bx, const vector<VarOrRVar> &by, const vector<VarOrRVar> &bz,
            const vector<VarOrRVar> &tx, const vector<VarOrRVar> &ty, const vector<VarOrRVar> &tz,
            const vector<Expr> &x_size, const vector<Expr> &y_size, const vector<Expr> &z_size) {
        add_mapper([=]{
            cs->gpu_tile(x[cp[0]], y[cp[1]], z[cp[2]], bx[cp[3]], by[cp[4]], bz[cp[5]], tx[cp[6]], ty[cp[7]], tz[cp[8]], x_size[cp[9]], y_size[cp[10]], z_size[cp[11]]);
        },
        make_index(__func__, x, y, z, bx, by, bz, tx, ty, tz, x_size, y_size, z_size));
        return *this;
    }

    HalideStageMapper &gpu_tile(const vector<VarOrRVar> &x, const vector<VarOrRVar> &y, const vector<VarOrRVar> &z,
            const vector<VarOrRVar> &tx, const vector<VarOrRVar> &ty, const vector<VarOrRVar> &tz,
            const vector<Expr> &x_size, const vector<Expr> &y_size, const vector<Expr> &z_size) {
        add_mapper([=]{
            cs->gpu_tile(x[cp[0]], y[cp[1]], z[cp[2]], tx[cp[3]], ty[cp[4]], tz[cp[5]], x_size[cp[6]], y_size[cp[7]], z_size[cp[8]]);
        },
        make_index(__func__, x, y, z, tx, ty, tz, x_size, y_size, z_size));
        return *this;
    }

};

class HalideFuncMapper : protected HalideStageMapper {
public:

    HalideFuncMapper &in(const Func &f) {
        // Can be used alone.
        add_mapper([&]{
            df = cf->in(f);
            cf = &df;
        },
        make_index(__func__, f));
        // Or can be used to schedule your own sub func.
        stringstream ss;
        ss << "." << __func__ << "(" << f.name() << ")";
        stack_mapper(ss.str(), [&]{
            df = cf->in(f);
            cf = &df;
        });
        return *this;
    }

    template<typename... Args>
    HalideFuncMapper &in(const Func &f1, const Func &f2, const Args&... args) {
        // Alone
        add_mapper([&]{
            df = cf->in({f1, f2, args...});
            cf = &df;
        },
        make_index(__func__, f1, f2, args...));
        // Sub func
        vector<Func> fs = {f1, f2, args...};
        stringstream ss;
        string separator;
        ss << "." << __func__ << "(";
        for (const Func &f : fs) {
            ss << f.name() << separator;
            separator = ", ";
        }
        ss << ")";
        stack_mapper(ss.str(), [&]{
            df = cf->in({f1, f2, args...});
            cf = &df;
        });
        return *this;
    }

    HalideFuncMapper &in() {
        // Alone
        add_mapper([&]{
            df = cf->in();
            cf = &df;
        },
        make_index(__func__));
        // Sub func
        stringstream ss;
        ss << "." << __func__ << "()";
        stack_mapper(ss.str(), [&]{
            df = cf->in();
            cf = &df;
        });
        return *this;
    }

    HalideFuncMapper &clone_in(const Func &f) {
        // Can be used alone.
        add_mapper([&]{
            df = cf->clone_in(f);
            cf = &df;
        },
        make_index(__func__, f));
        // Or can be used to schedule your own sub func.
        stringstream ss;
        ss << "." << __func__ << "(" << f.name() << ")";
        stack_mapper(ss.str(), [&]{
            df = cf->clone_in(f);
            cf = &df;
        });
        return *this;
    }

    template<typename... Args>
    HalideFuncMapper &clone_in(const Func &f1, const Func &f2, const Args&... args) {
        // Alone
        add_mapper([&]{
            df = cf->clone_in({f1, f2, args...});
            cf = &df;
        },
        make_index(__func__, f1, f2, args...));
        // Sub func
        vector<Func> fs = {f1, f2, args...};
        stringstream ss;
        string separator;
        ss << "." << __func__ << "(";
        for (const Func &f : fs) {
            ss << f.name() << separator;
            separator = ", ";
        }
        ss << ")";
        stack_mapper(ss.str(), [&]{
            df = cf->clone_in({f1, f2, args...});
            cf = &df;
        });
        return *this;
    }

    HalideFuncMapper &split(const vector<VarOrRVar> &old,
            const vector<VarOrRVar> &outer, const vector<VarOrRVar> &inner, const vector<Expr> &factor) {
        add_mapper([=]{
            cf->split(old[cp[0]], outer[cp[1]], inner[cp[2]], factor[cp[3]]);
        },
        make_index(__func__, old, outer, inner, factor));
        return *this;
    }

    HalideFuncMapper &split(const vector<VarOrRVar> &old_outer, const vector<VarOrRVar> &inner, const vector<Expr> &factor) {
        add_mapper([=]{
            cf->split(old_outer[cp[0]], old_outer[cp[0]], inner[cp[1]], factor[cp[2]]);
        },
        make_index(__func__, old_outer, inner, factor));
        return *this;
    }

    HalideFuncMapper &fuse(const vector<VarOrRVar> &inner, const vector<VarOrRVar> &outer, const vector<VarOrRVar> &fused) {
        add_mapper([=]{
            cf->fuse(inner[cp[0]], outer[cp[1]], fused[cp[2]]);
        },
        make_index(__func__, inner, outer, fused));
        return *this;
    }

    HalideFuncMapper &serial(const vector<VarOrRVar> &var) {
        add_mapper([=]{
            cf->serial(var[cp[0]]);
        },
        make_index(__func__, var));
        return *this;
    }

    HalideFuncMapper &parallel(const vector<VarOrRVar> &var) {
        add_mapper([=]{
            cf->parallel(var[cp[0]]);
        },
        make_index(__func__, var));
        return *this;
    }

    HalideFuncMapper &parallel(const vector<VarOrRVar> &var, const vector<Expr> &task_size) {
        add_mapper([=]{
            cf->parallel(var[cp[0]], task_size[cp[1]]);
        },
        make_index(__func__, var, task_size));
        return *this;
    }

    HalideFuncMapper &vectorize(const vector<VarOrRVar> &var) {
        add_mapper([=]{
            cf->vectorize(var[cp[0]]);
        },
        make_index(__func__, var));
        return *this;
    }

    HalideFuncMapper &vectorize(const vector<VarOrRVar> &var, const vector<Expr> &factor) {
        add_mapper([=]{
            cf->vectorize(var[cp[0]], factor[cp[1]]);
        },
        make_index(__func__, var, factor));
        return *this;
    }

    HalideFuncMapper &unroll(const vector<VarOrRVar> &var) {
        add_mapper([=]{
            cf->unroll(var[cp[0]]);
        },
        make_index(__func__, var));
        return *this;
    }

    HalideFuncMapper &unroll(const vector<VarOrRVar> &var, const vector<Expr> &factor) {
        add_mapper([=]{
            cf->unroll(var[cp[0]], factor[cp[1]]);
        },
        make_index(__func__, var, factor));
        return *this;
    }

    HalideFuncMapper &bound(const vector<Var> &var, const vector<Expr> &min, const vector<Expr> &extent) {
        add_mapper([=]{
            cf->bound(var[cp[0]], min[cp[1]], extent[cp[2]]);
        },
        make_index(__func__, var, min, extent));
        return *this;
    }

    HalideFuncMapper &bound_extent(const vector<Var> &var, const vector<Expr> &extent) {
        add_mapper([=]{
            cf->bound_extent(var[cp[0]], extent[cp[1]]);
        },
        make_index(__func__, var, extent));
        return *this;
    }

    HalideFuncMapper &tile(const vector<VarOrRVar> &x, const vector<VarOrRVar> &y,
            const vector<VarOrRVar> &xo, const vector<VarOrRVar> &yo,
            const vector<VarOrRVar> &xi, const vector<VarOrRVar> &yi,
            const vector<Expr> &xfactor, const vector<Expr> &yfactor) {
        add_mapper([=]{
            cf->tile(x[cp[0]], y[cp[1]], xo[cp[2]], yo[cp[3]], xi[cp[4]], yi[cp[5]], xfactor[cp[6]], yfactor[cp[7]]);
        },
        make_index(__func__, x, y, xo, yo, xi, yi, xfactor, yfactor));
        return *this;
    }

    HalideFuncMapper &tile(const vector<VarOrRVar> &x, const vector<VarOrRVar> &y,
            const vector<VarOrRVar> &xi, const vector<VarOrRVar> &yi,
            const vector<Expr> &xfactor, const vector<Expr> &yfactor) {
        add_mapper([=]{
            cf->tile(x[cp[0]], y[cp[1]], xi[cp[2]], yi[cp[3]], xfactor[cp[4]], yfactor[cp[5]]);
        },
        make_index(__func__, x, y, xi, yi, xfactor, yfactor));
        return *this;
    }

    template<typename... Args>
    HalideFuncMapper &reorder(const vector<VarOrRVar> &x, const vector<VarOrRVar> &y, const Args&... args) {
        add_mapper([=]{
            const int nargs = 2 + tuple_size<tuple<Args...>>::value;
            int* ap = cp + nargs;
            cf->reorder(x[cp[0]], y[cp[1]], args[*--ap]...);
        },
        make_index(__func__, x, y, args...));
        return *this;
    }

    template<typename... Args>
    HalideFuncMapper &reorder_storage(const vector<Var> &x, const vector<Var> &y, const Args&... args) {
        add_mapper([=]{
            const int nargs = 2 + tuple_size<tuple<Args...>>::value;
            int* ap = cp + nargs;
            cf->reorder_storage(x[cp[0]], y[cp[1]], args[*--ap]...);
        },
        make_index(__func__, x, y, args...));
        return *this;
    }

    HalideStageMapper &specialize(Expr condition) {
        // Can be used alone.
        add_mapper([=]{
            ds = cf->specialize(condition);
            cs = &ds;
        },
        make_index(__func__, condition));
        // Or can be used to schedule your own sub stage.
        stringstream ss;
        ss << "." << __func__ << "(" << condition << ")";
        stack_mapper(ss.str(), [=]{
            ds = cf->specialize(condition);
            cs = &ds;
        });
        return *this;
    }

    HalideFuncMapper &specialize_fail(const std::string &message) {
        add_mapper([=]{
            cf->specialize_fail(message);
        },
        make_index(__func__, message));
        return *this;
    }

    HalideFuncMapper &gpu_threads(const vector<VarOrRVar> &thread_x) {
        add_mapper([=]{
            cf->gpu_threads(thread_x[cp[0]]);
        },
        make_index(__func__, thread_x));
        return *this;
    }

    HalideFuncMapper &gpu_threads(const vector<VarOrRVar> &thread_x, const vector<VarOrRVar> &thread_y) {
        add_mapper([=]{
            cf->gpu_threads(thread_x[cp[0]], thread_y[cp[1]]);
        },
        make_index(__func__, thread_x, thread_y));
        return *this;
    }

    HalideFuncMapper &gpu_threads(const vector<VarOrRVar> &thread_x, const vector<VarOrRVar> &thread_y, const vector<VarOrRVar> &thread_z) {
        add_mapper([=]{
            cf->gpu_threads(thread_x[cp[0]], thread_y[cp[1]], thread_z[cp[2]]);
        },
        make_index(__func__, thread_x, thread_y, thread_z));
        return *this;
    }

    HalideFuncMapper &gpu_blocks(const vector<VarOrRVar> &block_x) {
        add_mapper([=]{
            cf->gpu_blocks(block_x[cp[0]]);
        },
        make_index(__func__, block_x));
        return *this;
    }

    HalideFuncMapper &gpu_blocks(const vector<VarOrRVar> &block_x, const vector<VarOrRVar> &block_y) {
        add_mapper([=]{
            cf->gpu_blocks(block_x[cp[0]], block_y[cp[1]]);
        },
        make_index(__func__, block_x, block_y));
        return *this;
    }

    HalideFuncMapper &gpu_blocks(const vector<VarOrRVar> &block_x, const vector<VarOrRVar> &block_y, const vector<VarOrRVar> &block_z) {
        add_mapper([=]{
            cf->gpu_blocks(block_x[cp[0]], block_y[cp[1]], block_z[cp[2]]);
        },
        make_index(__func__, block_x, block_y, block_z));
        return *this;
    }

    HalideFuncMapper &gpu(const vector<VarOrRVar> &block_x, const vector<VarOrRVar> &thread_x) {
        add_mapper([=]{
            cf->gpu(block_x[cp[0]], thread_x[cp[1]]);
        },
        make_index(__func__, block_x, thread_x));
        return *this;
    }

    HalideFuncMapper &gpu(const vector<VarOrRVar> &block_x, const vector<VarOrRVar> &block_y,
            const vector<VarOrRVar> &thread_x, const vector<VarOrRVar> &thread_y) {
        add_mapper([=]{
            cf->gpu(block_x[cp[0]], block_y[cp[1]], thread_x[cp[2]], thread_y[cp[3]]);
        },
        make_index(__func__, block_x, block_y, thread_x, thread_y));
        return *this;
    }

    HalideFuncMapper &gpu(const vector<VarOrRVar> &block_x, const vector<VarOrRVar> &block_y, const vector<VarOrRVar> &block_z,
            const vector<VarOrRVar> &thread_x, const vector<VarOrRVar> &thread_y, const vector<VarOrRVar> &thread_z) {
        add_mapper([=]{
            cf->gpu(block_x[cp[0]], block_y[cp[1]], block_z[cp[2]], thread_x[cp[3]], thread_y[cp[4]], thread_z[cp[5]]);
        },
        make_index(__func__, block_x, block_y, block_z, thread_x, thread_y, thread_z));
        return *this;
    }

    HalideFuncMapper &gpu_tile(const vector<VarOrRVar> &x,
            const vector<VarOrRVar> &bx, const vector<VarOrRVar> &tx, const vector<Expr> &x_size) {
        add_mapper([=]{
            cf->gpu_tile(x[cp[0]], bx[cp[1]], tx[cp[2]], x_size[cp[3]]);
        },
        make_index(__func__, x, bx, tx, x_size));
        return *this;
    }

    HalideFuncMapper &gpu_tile(const vector<VarOrRVar> &x, const vector<VarOrRVar> &tx, const vector<Expr> &x_size) {
        add_mapper([=]{
            cf->gpu_tile(x[cp[0]], tx[cp[1]], x_size[cp[2]]);
        },
        make_index(__func__, x, tx, x_size));
        return *this;
    }

    HalideFuncMapper &gpu_tile(const vector<VarOrRVar> &x, const vector<VarOrRVar> &y,
            const vector<VarOrRVar> &bx, const vector<VarOrRVar> &by,
            const vector<VarOrRVar> &tx, const vector<VarOrRVar> &ty,
            const vector<Expr> &x_size, const vector<Expr> &y_size) {
        add_mapper([=]{
            cf->gpu_tile(x[cp[0]], y[cp[1]], bx[cp[2]], by[cp[3]], tx[cp[4]], ty[cp[5]], x_size[cp[6]], y_size[cp[7]]);
        },
        make_index(__func__, x, y, bx, by, tx, ty, x_size, y_size));
        return *this;
    }

    HalideFuncMapper &gpu_tile(const vector<VarOrRVar> &x, const vector<VarOrRVar> &y,
            const vector<VarOrRVar> &tx, const vector<VarOrRVar> &ty,
            const vector<Expr> &x_size, const vector<Expr> &y_size) {
        add_mapper([=]{
            cf->gpu_tile(x[cp[0]], y[cp[1]], tx[cp[2]], ty[cp[3]], x_size[cp[4]], y_size[cp[5]]);
        },
        make_index(__func__, x, y, tx, ty, x_size, y_size));
        return *this;
    }

    HalideFuncMapper &gpu_tile(const vector<VarOrRVar> &x, const vector<VarOrRVar> &y, const vector<VarOrRVar> &z,
            const vector<VarOrRVar> &bx, const vector<VarOrRVar> &by, const vector<VarOrRVar> &bz,
            const vector<VarOrRVar> &tx, const vector<VarOrRVar> &ty, const vector<VarOrRVar> &tz,
            const vector<Expr> &x_size, const vector<Expr> &y_size, const vector<Expr> &z_size) {
        add_mapper([=]{
            cf->gpu_tile(x[cp[0]], y[cp[1]], z[cp[2]], bx[cp[3]], by[cp[4]], bz[cp[5]], tx[cp[6]], ty[cp[7]], tz[cp[8]], x_size[cp[9]], y_size[cp[10]], z_size[cp[11]]);
        },
        make_index(__func__, x, y, z, bx, by, bz, tx, ty, tz, x_size, y_size, z_size));
        return *this;
    }

    HalideFuncMapper &gpu_tile(const vector<VarOrRVar> &x, const vector<VarOrRVar> &y, const vector<VarOrRVar> &z,
            const vector<VarOrRVar> &tx, const vector<VarOrRVar> &ty, const vector<VarOrRVar> &tz,
            const vector<Expr> &x_size, const vector<Expr> &y_size, const vector<Expr> &z_size) {
        add_mapper([=]{
            cf->gpu_tile(x[cp[0]], y[cp[1]], z[cp[2]], tx[cp[3]], ty[cp[4]], tz[cp[5]], x_size[cp[6]], y_size[cp[7]], z_size[cp[8]]);
        },
        make_index(__func__, x, y, z, tx, ty, tz, x_size, y_size, z_size));
        return *this;
    }

    HalideFuncMapper &fold_storage(const vector<Var> &dim, const vector<Expr> &extent, bool fold_forward = true) {
        add_mapper([=]{
            cf->fold_storage(dim[cp[0]], extent[cp[1]], fold_forward);
        },
        make_index(__func__, dim, extent, fold_forward));
        return *this;
    }

    HalideFuncMapper &compute_at(const vector<Func> &f, const vector<Var> &var) {
        add_mapper([=]{
            cf->compute_at(f[cp[0]], var[cp[1]]);
        },
        make_index(__func__, f, var));
        return *this;
    }

    HalideFuncMapper &compute_at(const vector<Func> &f, const vector<RVar> &var) {
        add_mapper([=]{
            cf->compute_at(f[cp[0]], var[cp[1]]);
        },
        make_index(__func__, f, var));
        return *this;
    }

    HalideFuncMapper &compute_with(const vector<Stage> &s, const vector<VarOrRVar> &var) {
        add_mapper([=]{
            cf->compute_with(s[cp[0]], var[cp[1]]);
        },
        make_index(__func__, s, var));
        return *this;
    }

    HalideFuncMapper &compute_root() {
        add_mapper([=]{
            cf->compute_root();
        },
        make_index(__func__));
        return *this;
    }

    HalideFuncMapper &store_at(const vector<Func> &f, const vector<Var> &var) {
        add_mapper([=]{
            cf->store_at(f[cp[0]], var[cp[1]]);
        },
        make_index(__func__, f, var));
        return *this;
    }

    HalideFuncMapper &store_at(const vector<Func> &f, const vector<RVar> &var) {
        add_mapper([=]{
            cf->store_at(f[cp[0]], var[cp[1]]);
        },
        make_index(__func__, f, var));
        return *this;
    }

    HalideFuncMapper &store_root() {
        add_mapper([=]{
            cf->store_root();
        },
        make_index(__func__));
        return *this;
    }

    HalideFuncMapper &compute_inline() {
        add_mapper([=]{
            cf->compute_inline();
        },
        make_index(__func__));
        return *this;
    }

    HalideStageMapper &update(int idx = 0) {
        stringstream ss;
        ss << "." << __func__ << "(" << idx << ")";
        stack_mapper(ss.str(), [=]{
            ds = cf->update(idx);
            cs = &ds;
        });
        return *this;
    }

};

class HalideScheduleMapper : protected HalideFuncMapper {
public:

    /**
     * Always start mapping from a Func to control the Index of sub stages/funcs.
     */
    HalideFuncMapper &map(Func &f) {
        init_mapper();
        stack_mapper(f.name(), [&]{
            cf = &f;
        });
        return *this;
    }

};

// =============================================================

class HalideBaseAlgorithm {
public:

    /**
     * Invoked just once, before start running.
     * Define algorithm logic, but should NOT do any compilation.
     */
    virtual void define_algorithm() = 0;

    /**
     * When running, invoked before applying the selected schedule mappings.
     * Fixed initial schedule directives may be defined here.
     */
    virtual void schedule_preprocessor() {
    }

    /**
     * Invoked just once, before start running.
     * Define schedule directives options to consider when running.
     */
    virtual void schedule_mapping(HalideScheduleMapper &sm) = 0;

    /**
     * When running, invoked after applying the selected schedule mappings.
     * Fixed final schedule directives may be defined here.
     */
    virtual void schedule_postprocessor() {
    }

    /**
     * When running, invoked after select and process a new schedule.
     * Attention: Any outside compilation may lead to unknown behaviors.
     */
    virtual void realize_compilation() = 0;

    /**
     * When running, invoked after realize compilation.
     * Execution time is measured, NO extra task should be done.
     */
    virtual void realize_algorithm() = 0;

    /**
     * When running, invoked after realize algorithm (after each execution).
     * A previous processed image file may be used to validate algorithm result.
     */
    virtual bool realize_validation() {
        return true;
    }

};

class HalideBlur : public HalideBaseAlgorithm {
protected:

    const string image_path;
    Buffer<float> input = Tools::load_and_convert_image(image_path);
    Buffer<float> output = Buffer<float>(input.width(), input.height());
    Target target = get_target_from_environment();

    Func in_b, blur_x, blur_y;
    Var x, y, xo, yo, xi, yi;

public:

    HalideBlur(const string &image_path) : image_path(image_path),
            in_b("in_b"), blur_x("blur_x"), blur_y("blur_y"),
            x("x"), y("y"), xo("xo"), yo("yo"), xi("xi"), yi("yi") {
    }

    void define_algorithm() override {
        in_b = BoundaryConditions::repeat_edge(input);
        blur_x(x, y) = (in_b(x - 1, y) + in_b(x, y) + in_b(x + 1, y)) / 3.f;
        blur_y(x, y) = (blur_x(x, y - 1) + blur_x(x, y) + blur_x(x, y + 1)) / 3.f;
    }

    void schedule_mapping(HalideScheduleMapper &sm) override {
        vector<Expr> split_factor = {16, 32, 64, 128, 256, 512};
        vector<Expr> vecto_factor = {4, 8, 16};

        sm.map(blur_y)
            .tile({x}, {y}, {xi}, {yi}, split_factor, split_factor)
            .split({y},{yi}, split_factor)
            .vectorize({x, xi}, vecto_factor)
            .parallel({y});
        sm.map(blur_x)
            .compute_at({blur_y}, {x, yi})
            .store_at({blur_y}, {y})
            .vectorize({x}, vecto_factor);
    }

    void realize_compilation() override {
        blur_y.compile_jit(target);
    }

    void realize_algorithm() override {
        blur_y.realize(output, target);
    }

};

class HalideBlur2 : public HalideBlur {
public:

    HalideBlur2(const string &image_path) : HalideBlur(image_path) {
    }

    void schedule_mapping(HalideScheduleMapper &sm) override {
        vector<Expr> split_factor = {8, 16, 32, 64, 128, 256, 512};
        vector<Expr> vecto_factor = {4, 8, 16};
        vector<Expr> unroll_factor = {2, 3, 4};

        sm.map(blur_y)
            .bound({y}, {0}, {input.height()})
            .bound({x}, {0}, {input.width()})
            .compute_root()
            .tile({x}, {y}, {xi}, {yi}, split_factor, split_factor)
            .split({y}, {yi}, split_factor)
            .parallel({y})
            .unroll({xi, x}, unroll_factor)
            .vectorize({xi, x}, vecto_factor);
        sm.map(blur_x)
            .store_at({blur_y}, {y})
            .compute_at({blur_y}, {x, yi})
            .unroll({x}, unroll_factor)
            .vectorize({x}, vecto_factor);
    }

};

class HalideBlur3 : public HalideBlur {
public:

    HalideBlur3(const string &image_path) : HalideBlur(image_path) {
        target.set_feature(Target::CUDACapability35);
        target.set_feature(Target::CUDA);
    }

    void schedule_mapping(HalideScheduleMapper &sm) override {
        vector<Expr> split_factor = {8, 16, 32, 64, 128, 256, 512};
        vector<Expr> unroll_factor = {2, 3, 4};

        sm.map(blur_y)
            .bound({y}, {0}, {input.height()})
            .bound({x}, {0}, {input.width()})
            .compute_root()
            .gpu_tile({x}, {y}, {xi}, {yi}, split_factor, split_factor)
            .unroll({xi, yi}, unroll_factor);
        sm.map(blur_x)
            .store_at({blur_y}, {x, y})
            .compute_at({blur_y}, {x, y})
            .gpu_threads({x}, {y})
            .unroll({x, y}, unroll_factor);
    }

    void realize_algorithm() override {
        blur_y.realize(output, target);
        output.device_sync();
    }

};

class HalideHarris : public HalideBaseAlgorithm {
private:

    Expr sum3x3(Func f, Var x, Var y) {
        return f(x-1, y-1) + f(x-1, y) + f(x-1, y+1) +
                f(x, y-1)   + f(x, y)   + f(x, y+1) +
                f(x+1, y-1) + f(x+1, y) + f(x+1, y+1);
    }

protected:

    const string image_path;
    Buffer<float> input = Tools::load_and_convert_image(image_path);
    Buffer<float> output = Buffer<float>(input.width(), input.height());
    Target target = get_target_from_environment();

    Func in_b, gray, Iy, Ix, Ixx, Iyy, Ixy, Sxx, Syy, Sxy, det, trace, harris, shifted;
    Var x, y, xo, yo, xi, yi;

public:

    HalideHarris(const string &image_path) : image_path(image_path),
            in_b("in_b"), gray("gray"), Iy("Iy"), Ix("Ix"),
            Ixx("Ixx"), Iyy("Iyy"), Ixy("Ixy"), Sxx("Sxx"), Syy("Syy"), Sxy("Sxy"),
            det("det"), trace("trace"), harris("harris"), shifted("shifted"),
            x("x"), y("y"), xo("xo"), yo("yo"), xi("xi"), yi("yi") {
    }

    void define_algorithm() override {
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
    }

    void schedule_mapping(HalideScheduleMapper &sm) override {
        vector<Expr> split_factor = {16, 32, 64, 128, 256, 512};
        vector<Expr> vecto_factor = {4, 8, 16};

        sm.map(shifted)
            .tile({x}, {y}, {xi}, {yi}, split_factor, split_factor)
            .split({y},{yi}, split_factor)
            .vectorize({x, xi}, vecto_factor)
            .parallel({y});
        sm.map(harris)
            .compute_at({shifted}, {x, yi})
            .store_at({shifted}, {y})
            .vectorize({x}, vecto_factor);
        sm.map(trace)
            .compute_at({harris, shifted}, {x, yi})
            .store_at({harris, shifted}, {y})
            .vectorize({x}, vecto_factor);
        sm.map(det)
            .compute_at({trace, harris, shifted}, {x, yi})
            .store_at({trace, harris, shifted}, {y})
            .vectorize({x}, vecto_factor);
        sm.map(Ix)
            .compute_at({det, trace, harris, shifted}, {x, yi})
            .store_at({det, trace, harris, shifted}, {y})
            .vectorize({x}, vecto_factor);
        sm.map(Iy)
            .compute_at({det, trace, harris, shifted}, {x, yi})
            .store_at({det, trace, harris, shifted}, {y})
            .vectorize({x}, vecto_factor);
        sm.map(Ix)
            .tile({x}, {y}, {xi}, {yi}, split_factor, split_factor)
            .split({y},{yi}, split_factor)
            .vectorize({x, xi}, vecto_factor)
            .parallel({y});
        sm.map(Iy)
            .compute_at({Ix}, {x, yi})
            .store_at({Ix}, {y})
            .vectorize({x}, vecto_factor);
    }

    void realize_compilation() override {
        shifted.compile_jit(target);
    }

    void realize_algorithm() override {
        shifted.realize(output, target);
    }

};

class HalideHarris2 : public HalideHarris {
public:

    HalideHarris2(const string &image_path) : HalideHarris(image_path) {
    }

    void schedule_mapping(HalideScheduleMapper &sm) override {
        vector<Expr> split_factor = {16, 32, 64, 128, 256, 512};
        vector<Expr> vecto_factor = {4, 8, 16};
        vector<Expr> vecto_factor2 = {16, 32, 64};

        sm.map(shifted)
            .compute_root()
            .tile({x}, {y}, {xi}, {yi}, split_factor, split_factor)
            .vectorize({xi}, vecto_factor)
            .tile({x}, {y}, {xi}, {yi}, vecto_factor, split_factor)
            .vectorize({xi})
            .split({y}, {yi}, split_factor)
            .vectorize({x}, vecto_factor)
            .parallel({y});
        sm.map(Ix)
            .compute_at({shifted}, {x, y})
            .vectorize({x}, vecto_factor);
        sm.map(Iy)
            .compute_at({shifted}, {x, y})
            .vectorize({x}, vecto_factor);
        sm.map(gray)
            .compute_at({shifted}, {x, y})
            .vectorize({x}, vecto_factor);
        sm.map(in_b)
            .compute_at({shifted}, {x, y})
            .vectorize({_0}, vecto_factor2);
    }

};

class HalideHarris3 : public HalideHarris {
public:

    HalideHarris3(const string &image_path) : HalideHarris(image_path) {
    }

    void schedule_mapping(HalideScheduleMapper &sm) override {
        vector<Expr> split_factor = {8, 16, 32, 64, 128, 256, 512};
        vector<Expr> vecto_factor = {4, 8, 16};
        vector<Expr> unroll_factor = {2, 3, 4};

        sm.map(shifted)
            .bound({y}, {0}, {input.height()})
            .bound({x}, {0}, {input.width()})
            .compute_root()
            .tile({x}, {y}, {xi}, {yi}, split_factor, split_factor)
            .split({y}, {yi}, split_factor)
            .parallel({y})
            .unroll({xi, x}, unroll_factor)
            .vectorize({xi, x}, vecto_factor);
        sm.map(gray)
            .store_at({shifted}, {y})
            .compute_at({shifted}, {x, yi})
            .unroll({x}, unroll_factor)
            .vectorize({x}, vecto_factor);
        sm.map(Ix)
            .store_at({shifted}, {y})
            .compute_at({shifted}, {x, yi})
            .unroll({x}, unroll_factor)
            .vectorize({x}, vecto_factor);
        sm.map(Iy)
            .store_at({shifted}, {y})
            .compute_at({shifted}, {x, yi})
            .unroll({x}, unroll_factor)
            .vectorize({x}, vecto_factor);
    }

};

class HalideHarris4 : public HalideHarris {
public:

    HalideHarris4(const string &image_path) : HalideHarris(image_path) {
        target.set_feature(Target::CUDACapability35);
        target.set_feature(Target::CUDA);
    }

    void schedule_mapping(HalideScheduleMapper &sm) override {
        vector<Expr> split_factor = {8, 16, 32, 64, 128, 256, 512};
        vector<Expr> unroll_factor = {2, 3, 4};

        sm.map(shifted)
            .bound({y}, {0}, {input.height()})
            .bound({x}, {0}, {input.width()})
            .compute_root()
            .gpu_tile({x}, {y}, {xi}, {yi}, split_factor, split_factor)
            .unroll({xi, yi}, unroll_factor);
        sm.map(gray)
            .store_at({shifted}, {x, y})
            .compute_at({shifted}, {x, y})
            .gpu_threads({x}, {y})
            .unroll({x, y}, unroll_factor);
        sm.map(Ix)
            .store_at({shifted}, {x, y})
            .compute_at({shifted}, {x, y})
            .gpu_threads({x}, {y})
            .unroll({x, y}, unroll_factor);
        sm.map(Iy)
            .store_at({shifted}, {x, y})
            .compute_at({shifted}, {x, y})
            .gpu_threads({x}, {y})
            .unroll({x, y}, unroll_factor);
    }

    void realize_algorithm() override {
        shifted.realize(output, target);
        output.device_sync();
    }

};

class HalideInterpolate : public HalideBaseAlgorithm {
protected:

    const string image_path;
    Buffer<float> input = Tools::load_and_convert_image(image_path);
    Buffer<float> output = Buffer<float>(input.width(), input.height(), 3);
    Target target = get_target_from_environment();

    Func in_b, normalize;
    Var x, y, c, xo, yo, co, xi, yi, ci;

    static const int levels = 10;
    Func downsampled[levels];
    Func downx[levels];
    Func interpolated[levels];
    Func upsampled[levels];
    Func upsampledx[levels];

public:

    HalideInterpolate(const string &image_path) : image_path(image_path),
            in_b("in_b"), normalize("normalize"),
            x("x"), y("y"), c("c"), xo("xo"), yo("yo"), co("co"), xi("xi"), yi("yi"), ci("ci"),
            downsampled{Func("downsampled[0]"),Func("downsampled[1]"),Func("downsampled[2]"),Func("downsampled[3]"),Func("downsampled[4]"),Func("downsampled[5]"),Func("downsampled[6]"),Func("downsampled[7]"),Func("downsampled[8]"),Func("downsampled[9]")},
            downx{Func("downx[0]"),Func("downx[1]"),Func("downx[2]"),Func("downx[3]"),Func("downx[4]"),Func("downx[5]"),Func("downx[6]"),Func("downx[7]"),Func("downx[8]"),Func("downx[9]")},
            interpolated{Func("interpolated[0]"),Func("interpolated[1]"),Func("interpolated[2]"),Func("interpolated[3]"),Func("interpolated[4]"),Func("interpolated[5]"),Func("interpolated[6]"),Func("interpolated[7]"),Func("interpolated[8]"),Func("interpolated[9]")},
            upsampled{Func("upsampled[0]"),Func("upsampled[1]"),Func("upsampled[2]"),Func("upsampled[3]"),Func("upsampled[4]"),Func("upsampled[5]"),Func("upsampled[6]"),Func("upsampled[7]"),Func("upsampled[8]"),Func("upsampled[9]")},
            upsampledx{Func("upsampledx[0]"),Func("upsampledx[1]"),Func("upsampledx[2]"),Func("upsampledx[3]"),Func("upsampledx[4]"),Func("upsampledx[5]"),Func("upsampledx[6]"),Func("upsampledx[7]"),Func("upsampledx[8]"),Func("upsampledx[9]")} {
    }

    void define_algorithm() override {
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
    }

    void schedule_preprocessor() {
        // Initial schedule needed to compile
        for (int l = 1; l < levels-1; ++l) {
            downsampled[l].compute_root();
            interpolated[l].compute_root();
        }
    }

    void schedule_mapping(HalideScheduleMapper &sm) override {
        vector<Expr> split_factor = {16, 32, 64, 128, 256, 512};
        vector<Expr> vecto_factor = {4, 8, 16};

        sm.map(normalize)
            .compute_root()
            .reorder({c}, {x}, (vector<VarOrRVar>){y})
            .tile({x}, {y}, {xi}, {yi}, split_factor, split_factor)
            .tile({x}, {y}, {xi}, {yi}, vecto_factor, vecto_factor)
            .bound({c}, {0}, {3})
            .unroll({c, xi, yi})
            .unroll({x, y}, vecto_factor)
            .parallel({y, c})
            .parallel({y}, split_factor)
            .vectorize({x, c})
            .vectorize({x}, vecto_factor)
            .bound({x}, {0}, {input.width()})
            .bound({y}, {0}, {input.height()});
        for (int l = 1; l < levels-1; ++l) {
            sm.map(interpolated[l])
                .reorder({c}, {x}, (vector<VarOrRVar>){y})
                .tile({x}, {y}, {xi}, {yi}, split_factor, split_factor)
                .tile({x}, {y}, {xi}, {yi}, vecto_factor, vecto_factor)
                .bound({c}, {0}, {3})
                .unroll({c, xi, yi})
                .unroll({x, y}, vecto_factor)
                .parallel({y, c})
                .parallel({y}, split_factor)
                .vectorize({x, c})
                .vectorize({x}, vecto_factor);
            sm.map(downsampled[l])
                .reorder({c}, {x}, (vector<VarOrRVar>){y})
                .tile({x}, {y}, {xi}, {yi}, split_factor, split_factor)
                .tile({x}, {y}, {xi}, {yi}, vecto_factor, vecto_factor)
                .bound({c}, {0}, {3})
                .unroll({c, xi, yi})
                .unroll({x, y}, vecto_factor)
                .parallel({y, c})
                .parallel({y}, split_factor)
                .vectorize({x, c})
                .vectorize({x}, vecto_factor);
        }
        sm.map(in_b)
            .compute_root()
            .reorder({_2}, {_0}, (vector<VarOrRVar>){_1})
            .parallel({_2, _0})
            .vectorize({_0, _2}, vecto_factor)
            .bound({_2}, {0}, {4})
            .bound_extent({_2}, {4})
            .unroll({_2});
    }

    void realize_compilation() override {
        normalize.compile_jit(target);
    }

    void realize_algorithm() override {
        normalize.realize(output, target);
    }

};

class HalideInterpolate2 : public HalideInterpolate {
public:

    HalideInterpolate2(const string &image_path) : HalideInterpolate(image_path) {
    }

    void schedule_mapping(HalideScheduleMapper &sm) override {
        vector<Expr> split_factor = {8, 16, 32, 64, 128, 256, 512};
        vector<Expr> vecto_factor = {4, 8, 16};
        vector<Expr> unroll_factor = {2, 3, 4};

        for (int l = 1; l < levels-1; ++l) {
            sm.map(downsampled[l])
                .tile({x}, {y}, {xi}, {yi}, split_factor, split_factor)
                .split({y}, {yi}, split_factor)
                .parallel({y, c})
                .unroll({xi, x, c}, unroll_factor)
                .vectorize({xi, x}, vecto_factor);
            sm.map(downx[l])
                .store_at({downsampled[l]}, {y})
                .compute_at({downsampled[l]}, {x, yi})
                .unroll({x}, unroll_factor)
                .vectorize({x}, vecto_factor);
            sm.map(interpolated[l])
                .tile({x}, {y}, {xi}, {yi}, split_factor, split_factor)
                .split({y}, {yi}, split_factor)
                .parallel({y, c})
                .unroll({xi, x, c}, unroll_factor)
                .vectorize({xi, x}, vecto_factor);
            sm.map(interpolated[l])
                .store_at({normalize}, {y})
                .compute_at({normalize}, {x, yi});
            sm.map(upsampledx[l])
                .store_at({normalize}, {y})
                .compute_at({normalize}, {x, yi})
                .unroll({x}, unroll_factor)
                .vectorize({x}, vecto_factor);
        }
        sm.map(normalize)
            .bound({c}, {0}, {3})
            .bound({y}, {0}, {input.height()})
            .bound({x}, {0}, {input.width()})
            .compute_root()
            .tile({x}, {y}, {xi}, {yi}, split_factor, split_factor)
            .split({y}, {yi}, split_factor)
            .parallel({y, c})
            .unroll({xi, x, c}, unroll_factor)
            .vectorize({xi, x}, vecto_factor);
    }

};

class HalideInterpolate3 : public HalideInterpolate {
public:

    HalideInterpolate3(const string &image_path) : HalideInterpolate(image_path) {
        target.set_feature(Target::CUDACapability35);
        target.set_feature(Target::CUDA);
    }

    void schedule_mapping(HalideScheduleMapper &sm) override {
        vector<Expr> split_factor = {8, 16, 32, 64, 128, 256, 512};
        vector<Expr> unroll_factor = {2, 3, 4};

        for (int l = 1; l < levels-1; ++l) {
            sm.map(downsampled[l])
                .gpu_tile({x}, {y}, {c}, {xi}, {yi}, {ci}, split_factor, split_factor, {4})
                .unroll({xi, yi, ci}, unroll_factor);
            sm.map(downx[l])
                .store_at({downsampled[l]}, {x, y, c})
                .compute_at({downsampled[l]}, {x, y, c})
                .gpu_threads({x}, {y}, {c})
                .unroll({x, y, c}, unroll_factor);
            sm.map(interpolated[l])
                .gpu_tile({x}, {y}, {c}, {xi}, {yi}, {ci}, split_factor, split_factor, {4})
                .unroll({xi, yi, ci}, unroll_factor);
            sm.map(interpolated[l])
                .store_at({normalize}, {x, y, c})
                .compute_at({normalize}, {x, y, c})
                .gpu_threads({x}, {y}, {c})
                .unroll({x, y, c}, unroll_factor);
            sm.map(upsampledx[l])
                .store_at({normalize}, {x, y, c})
                .compute_at({normalize}, {x, y, c})
                .gpu_threads({x}, {y}, {c})
                .unroll({x, y, c}, unroll_factor);
        }
        sm.map(normalize)
            .bound({c}, {0}, {3})
            .bound({y}, {0}, {input.height()})
            .bound({x}, {0}, {input.width()})
            .compute_root()
            .gpu_tile({x}, {y}, {c}, {xi}, {yi}, {ci}, split_factor, split_factor, {3})
            .unroll({xi, yi, ci}, unroll_factor);
    }

    void realize_algorithm() override {
        normalize.realize(output, target);
        output.device_sync();
    }

};

// =============================================================

class HalideRLScheduler {
private:

    HalideBaseMapper *mapper;
    HalideBaseAlgorithm *algorithm;

    HalideBaseMapper::MapperOperation **schedule = nullptr;
    int schedule_size = 0;
    int schedule_count = 0;
    string schedule_str;

    int max_stage_directive = 0;
    std::map<int,int> stage_count;

    static const int COMPILE_TIMEOUT_SEC = 10;
    static const int EXEC_TIMEOUT_SEC = 3;
    static const int EXEC_SAMPLE_LIMIT = 3;

    static const int EXIT_OK = 255;
    static const int EXIT_TIMEOUT = 254;
    static const int EXIT_ERROR = 1;
    static const int EXIT_TIMEOUT_ERROR = 2;

    double *shr_exec_time_sec;

    static void compile_timeout_signal_handler(int signal) {
        _Exit(EXIT_TIMEOUT_ERROR); // async-signal-safe
    }

    static void realize_timeout_signal_handler(int signal) {
        _Exit(EXIT_TIMEOUT); // async-signal-safe
    }

    void apply_schedule() {
        algorithm->schedule_preprocessor();
        for (int i = 0; i < schedule_count; i++) {
            mapper->apply(*schedule[i]);
        }
        algorithm->schedule_postprocessor();
    }

    void exec_schedule(bool &exec_error, bool &exec_timeout, double &exec_time_sec) {
        pid_t pid = fork();
        if (pid == 0) {
            try {
                // Hide halide error messages
                cerr.setstate(ios::eofbit);

                apply_schedule();

                // Compilation - Time limit if too slow or stuck

                __sighandler_t sh = signal(SIGALRM, compile_timeout_signal_handler);
                confirm(sh != SIG_ERR, "Can't set signal alarm handler");

                timer_t timer_id;
                int ret = timer_create(CLOCK_REALTIME, NULL, &timer_id);
                confirm(ret == 0, "Can't create timer signal alarm");

                itimerspec ts = {0};
                ts.it_value = {COMPILE_TIMEOUT_SEC, 0};
                ret = timer_settime(timer_id, 0, &ts, NULL);
                confirm(ret == 0, "Can't start timer signal alarm");

                # if DEBUG
                    long start1 = chrono::duration_cast< chrono::milliseconds >(chrono::system_clock::now().time_since_epoch()).count();
                # endif

                algorithm->realize_compilation();

                # if DEBUG
                    long end1 = chrono::duration_cast< chrono::milliseconds >(chrono::system_clock::now().time_since_epoch()).count();
                    cout << " compile ms " << (end1 - start1);
                    cout.flush();
                # endif

                ret = timer_delete(timer_id);
                confirm(ret == 0, "Can't stop timer signal alarm");

                // Execution - Time limit if too slow (bad reward)

                sh = signal(SIGALRM, realize_timeout_signal_handler);
                confirm(sh != SIG_ERR, "Can't set signal alarm handler");

                ret = timer_create(CLOCK_REALTIME, NULL, &timer_id);
                confirm(ret == 0, "Can't create timer signal alarm");

                ts = {0};
                ts.it_value = {EXEC_TIMEOUT_SEC, 0};
                ret = timer_settime(timer_id, 0, &ts, NULL);
                confirm(ret == 0, "Can't start timer signal alarm");

                # if DEBUG
                    long start2 = chrono::duration_cast< chrono::milliseconds >(chrono::system_clock::now().time_since_epoch()).count();
                # endif

                *shr_exec_time_sec = Tools::benchmark(EXEC_SAMPLE_LIMIT, 1, [&]{
                    algorithm->realize_algorithm();
                });

                # if DEBUG
                    long end2 = chrono::duration_cast< chrono::milliseconds >(chrono::system_clock::now().time_since_epoch()).count();
                    cout << " exec ms " << (end2 - start2);
                    cout.flush();
                # endif

                ret = timer_delete(timer_id);
                confirm(ret == 0, "Can't stop timer signal alarm");

                _Exit(algorithm->realize_validation() ? EXIT_OK : EXIT_ERROR);
            } catch (...) {
                abort();
            }
        } else if (pid > 0) {
            int status = 0;
            wait(&status);
            if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_OK) {
                exec_error = false;
                exec_timeout = false;
                exec_time_sec = *shr_exec_time_sec;
            } else if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_TIMEOUT) {
                exec_error = false;
                exec_timeout = true;
                exec_time_sec = EXEC_TIMEOUT_SEC;
            } else if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_TIMEOUT_ERROR) {
                exec_error = true;
                exec_timeout = true;
                exec_time_sec = 0.0;
            } else {
                exec_error = true;
                exec_timeout = false;
                exec_time_sec = 0.0;
            }
        } else {
            throw runtime_error("Fork failed, can't create child process");
        }
    }

public:

    HalideRLScheduler(HalideScheduleMapper *mapper, HalideBaseAlgorithm *algorithm) :
            mapper(reinterpret_cast<HalideBaseMapper*>(mapper)),
            algorithm(algorithm) {
        shr_exec_time_sec = (double *) mmap(NULL, sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        confirm(shr_exec_time_sec != MAP_FAILED, "Can't create mapping shared memory");
    }

    virtual ~HalideRLScheduler() {
        if (schedule != nullptr) {
            for (int i = 0; i < schedule_size; i++)
                delete schedule[i];
            delete[] schedule;
        }
        munmap(shr_exec_time_sec, sizeof(double));
    }

    void init(const ScheduleInitRequest *req, ScheduleInitResponse *ret) {
        if (mapper->max_directive() > 0)
            throw logic_error("Scheduler already initialized");

        algorithm->define_algorithm();
        algorithm->schedule_mapping(*reinterpret_cast<HalideScheduleMapper*>(mapper));

        if (mapper->max_directive() <= 0)
            throw logic_error("No schedule mapping options defined");

        max_stage_directive = req->max_stage_directive();
        schedule_str.reserve(200);
        schedule_count = 0;
        schedule_size = mapper->max_stage() * req->max_stage_directive() + 1;
        schedule = new HalideBaseMapper::MapperOperation*[schedule_size];
        for (int i = 0; i < schedule_size; i++)
            schedule[i] = new HalideBaseMapper::MapperOperation(mapper->max_param());

        bool exec_error, exec_timeout;
        double exec_time_sec;
        exec_schedule(exec_error, exec_timeout, exec_time_sec);

        if (exec_error || exec_timeout)
            throw logic_error("Default schedule execution failed, can't define initial exec time, error/timeout " +
                    to_string(exec_error) + "/" + to_string(exec_timeout));

        ret->set_max_stage(mapper->max_stage());
        ret->set_max_directive(mapper->max_directive());
        ret->set_max_param(mapper->max_param());
        ret->set_schedule_map_range(mapper->map_range());
        ret->set_init_time_sec(exec_time_sec);
    }

    void step(const ScheduleStepRequest *req, ScheduleStepResponse *ret) {
        HalideBaseMapper::MapperOperation &op = *schedule[schedule_count];
        const OperationRequest &oreq = req->op();
        mapper->decode(oreq.map_code(), op);
        OperationResponse *oret = ret->mutable_op();
        oret->add_elem_id(op.stage_index);
        oret->add_elem_id(op.directive_index);
        for (int i = 0; i < mapper->max_param(); i++)
            oret->add_elem_id(op.param_index[i]);

        schedule_count++;
        stage_count[op.stage_index] += 1;
        bool bound = stage_count[op.stage_index] <= max_stage_directive;

        # if DEBUG
            cout << "debug: map_code " << req->op().map_code() << " count " << schedule_count;
            cout.flush();
            long start = chrono::duration_cast< chrono::milliseconds >(chrono::system_clock::now().time_since_epoch()).count();
        # endif

        bool exec_error, exec_timeout;
        double exec_time_sec;
        if (bound) {
            exec_schedule(exec_error, exec_timeout, exec_time_sec);
        } else {
            exec_error = true;
            exec_timeout = false;
            exec_time_sec = 0.0;
        }

        # if DEBUG
            long end = chrono::duration_cast< chrono::milliseconds >(chrono::system_clock::now().time_since_epoch()).count();
            cout << " total ms " << (end - start) << " error/timeout " << exec_error << "/" << exec_timeout << " ret exec sec " << exec_time_sec << endl;
            cout.flush();
        # endif

        if (exec_error) {
            schedule_count--;
            stage_count[op.stage_index] -= 1;
        }

        ret->set_exec_error(exec_error);
        ret->set_exec_timeout(exec_timeout);
        ret->set_exec_time_sec(exec_time_sec);
    }

    void reset(const ScheduleResetRequest *req, ScheduleResetResponse *ret) {
        schedule_count = 0;
        stage_count.clear();
        HalideBaseMapper::MapperOperation &op = *schedule[0];
        for (int i = 0; i < req->op_size(); i++) {
            const OperationRequest &oreq = req->op(i);
            mapper->decode(oreq.map_code(), op);
            OperationResponse *oret = ret->add_op();
            oret->add_elem_id(op.stage_index);
            oret->add_elem_id(op.directive_index);
            for (int j = 0; j < mapper->max_param(); j++)
                oret->add_elem_id(op.param_index[j]);
        }
    }

    void render(const ScheduleRenderRequest *req, ScheduleRenderResponse *ret) {
        for (int i = 0; i < schedule_count; i++) {
            schedule_str.clear();
            mapper->render(*schedule[i], schedule_str);
            if (!schedule_str.empty())
                ret->add_schedule_str(schedule_str);
        }
    }

};

// =============================================================

class ScheduleServiceImpl final : public ScheduleService::Service {
private:

    HalideBaseAlgorithm *algorithm = nullptr;
    HalideScheduleMapper *mapper = nullptr;
    HalideRLScheduler *scheduler = nullptr;

    void release() {
        delete scheduler;
        delete mapper;
        delete algorithm;
        scheduler = nullptr;
        mapper = nullptr;
        algorithm = nullptr;
    }

public:

    Status init(ServerContext *context, const ScheduleInitRequest *request, ScheduleInitResponse *response) override {
        try {
            release();
            string image_path = "../resource/" + request->input_image();
            switch (request->algorithm_id()) {
                case 10:
                    algorithm = new HalideBlur(image_path); break;
                case 11:
                    algorithm = new HalideBlur2(image_path); break;
                case 12:
                    algorithm = new HalideBlur3(image_path); break;
                case 20:
                    algorithm = new HalideHarris(image_path); break;
                case 21:
                    algorithm = new HalideHarris2(image_path); break;
                case 22:
                    algorithm = new HalideHarris3(image_path); break;
                case 23:
                    algorithm = new HalideHarris4(image_path); break;
                case 30:
                    algorithm = new HalideInterpolate(image_path); break;
                case 31:
                    algorithm = new HalideInterpolate2(image_path); break;
                case 32:
                    algorithm = new HalideInterpolate3(image_path); break;
                default:
                    throw domain_error("Algorithm id " + to_string(request->algorithm_id()) + " undefined");
            }
            mapper = new HalideScheduleMapper();
            scheduler = new HalideRLScheduler(mapper, algorithm);
            scheduler->init(request, response);
        } catch (const std::exception &ex) {
            release();
            return Status(StatusCode::ABORTED, ex.what());
        }
        return Status::OK;
    }

    Status step(ServerContext *context, const ScheduleStepRequest *request, ScheduleStepResponse *response) override {
        try {
            scheduler->step(request, response);
        } catch (const std::exception &ex) {
            release();
            return Status(StatusCode::ABORTED, ex.what());
        }
        return Status::OK;
    }

    Status reset(ServerContext *context, const ScheduleResetRequest *request, ScheduleResetResponse *response) override {
        try {
            scheduler->reset(request, response);
        } catch (const std::exception &ex) {
            release();
            return Status(StatusCode::ABORTED, ex.what());
        }
        return Status::OK;
    }

    Status render(ServerContext *context, const ScheduleRenderRequest *request, ScheduleRenderResponse *response) override {
        try {
            scheduler->render(request, response);
        } catch (const std::exception &ex) {
            release();
            return Status(StatusCode::ABORTED, ex.what());
        }
        return Status::OK;
    }

    Status close(ServerContext *context, const ScheduleCloseRequest *request, ScheduleCloseResponse *response) override {
        release();
        return Status::OK;
    }

};

void RunServer(const std::string server_address) {
    ScheduleServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
    std::cout << "Server shuts down" << std::endl;
}

//#include "test.hpp"

// =============================================================

int main(int argc, char** argv) {
    std::string server_address("localhost:50051");
    if (argc > 1) {
        server_address.assign(argv[1]);
    }

    RunServer(server_address);
    return 0;
}
