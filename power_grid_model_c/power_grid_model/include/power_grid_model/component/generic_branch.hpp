// SPDX-FileCopyrightText: Contributors to the Power Grid Model project <powergridmodel@lfenergy.org>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "branch.hpp"

#include "../auxiliary/input.hpp"
#include "../auxiliary/output.hpp"
#include "../auxiliary/update.hpp"
#include "../calculation_parameters.hpp"
#include "../common/common.hpp"
#include "../common/exception.hpp"
#include "../common/three_phase_tensor.hpp"

/*
    generic Branch: either a line N = 1 or a transformer N = t*e^(j*theta)
    paramaters should be given as r1, x1, ....

     -----| |-----------y1_series-------
          | |   |                 |
          | |   y1_shunt          y1_shunt
          | |   |                 |
          | |   |                 |
     -----| |--------------------------
          N = k * e^(j*theta)

*/

namespace power_grid_model {

class GenericBranch final : public Branch {
  public:
    using InputType = GenericBranchInput;
    using UpdateType = BranchUpdate;
    static constexpr char const* name = "generic_branch";

    explicit GenericBranch(GenericBranchInput const& genericbranch_input, double u1_rated, double u2_rated)
        : Branch{genericbranch_input},
          sn_{genericbranch_input.sn},
          r1_{genericbranch_input.r1},
          x1_{genericbranch_input.x1},
          g1_{genericbranch_input.g1},
          b1_{genericbranch_input.b1},
          k_{std::isnan(genericbranch_input.k) ? 1.0 : genericbranch_input.k},
          theta_{std::isnan(genericbranch_input.theta) ? 0.0 : std::fmod(genericbranch_input.theta, 2 * pi)},
          base_i_from_{base_power_3p / u1_rated / sqrt3},
          base_i_to_{base_power_3p / u2_rated / sqrt3},
          base_y_{base_i_to_ / (u2_rated / sqrt3)} {
        y1_series_ = 1.0 / (r1_ + 1.0i * x1_) / base_y_;
        y1_shunt_ = (g1_ + 1.0i * b1_) / base_y_;
    }

    // override getter
    double base_i_from() const override { return base_i_from_; }
    double base_i_to() const override { return base_i_to_; }
    double loading(double max_s, double /*max_i*/) const override { return std::isnan(sn_) ? 0.0 : (max_s / sn_); };
    double phase_shift() const override { return theta_; }
    bool is_param_mutable() const override { return false; }

    template <symmetry_tag sym>
    BranchOutput<sym> get_output(BranchSolverOutput<sym> const& branch_solver_output) const {
        // result object
        BranchOutput<sym> output{};
        static_cast<BaseOutput&>(output) = base_output(true);
        // calculate result
        output.p_from = base_power<sym> * real(branch_solver_output.s_f);
        output.q_from = base_power<sym> * imag(branch_solver_output.s_f);
        output.i_from = base_i_from() * cabs(branch_solver_output.i_f);
        output.s_from = base_power<sym> * cabs(branch_solver_output.s_f);
        output.p_to = base_power<sym> * real(branch_solver_output.s_t);
        output.q_to = base_power<sym> * imag(branch_solver_output.s_t);
        output.i_to = base_i_to() * cabs(branch_solver_output.i_t);
        output.s_to = base_power<sym> * cabs(branch_solver_output.s_t);
        double const max_s = std::max(sum_val(output.s_from), sum_val(output.s_to));
        double const max_i = std::max(max_val(output.i_from), max_val(output.i_to));
        output.loading = loading(max_s, max_i);
        output.shift = theta_;
        output.ratio = k_;

        output.r1 = r1_;
        output.x1 = x1_;
        output.b1 = b1_;
        output.g1 = g1_;

        return output;
    }

  private:
    double sn_;
    double r1_;
    double x1_;
    double g1_;
    double b1_;
    double k_;
    double theta_;
    double base_i_from_;
    double base_i_to_;
    double base_y_;
    DoubleComplex y1_series_;
    DoubleComplex y1_shunt_;

    BranchCalcParam<symmetric_t> sym_calc_param() const override {
        return calc_param_y_sym(y1_series_, y1_shunt_, k_ * std::exp(1.0i * theta_));
    }

    [[noreturn]] BranchCalcParam<asymmetric_t> asym_calc_param() const override { throw NotImplementedError(); }
};

} // namespace power_grid_model
