// SPDX-FileCopyrightText: Contributors to the Power Grid Model project <powergridmodel@lfenergy.org>
//
// SPDX-License-Identifier: MPL-2.0

#include "power_grid_model_cpp.hpp"

#include <power_grid_model_c/dataset_definitions.h>

#include <doctest/doctest.h>

#include <array>
#include <exception>
#include <limits>
#include <string>

/*
Testing network

source_1(1.0 p.u., 100.0 V) --internal_impedance(j10.0 ohm, sk=1000.0 VA, rx_ratio=0.0)--
-- node_0 (100.0 V) --load_2(const_i, -j5.0A, 0.0 W, 500.0 var)

u0 = 100.0 V - (j10.0 ohm * -j5.0 A) = 50.0 V

update_0:
    u_ref = 0.5 p.u. (50.0 V)
    q_specified = 100 var (-j1.0A)
u0 = 50.0 V - (j10.0 ohm * -j1.0 A) = 40.0 V

update_1:
    q_specified = 300 var (-j3.0A)
u0 = 100.0 V - (j10.0 ohm * -j3.0 A) = 70.0 V
*/

namespace power_grid_model_cpp {
namespace {
void check_exception(PowerGridError const& e, PGM_ErrorCode const& reference_error,
                     std::string_view reference_err_msg) {
    CHECK(e.error_code() == reference_error);
    std::string const err_msg{e.what()};
    doctest::String const ref_err_msg{reference_err_msg.data()};
    REQUIRE(err_msg.c_str() == doctest::Contains(ref_err_msg));
}

template <typename Func, class... Args>
void check_throws_with(Func&& func, PGM_ErrorCode const& reference_error, std::string_view reference_err_msg,
                       Args&&... args) {
    try {
        std::forward<Func>(func)(std::forward<Args>(args)...);
        FAIL("Expected error not thrown.");
    } catch (PowerGridRegularError const& e) {
        check_exception(e, reference_error, reference_err_msg);
    }
}
} // namespace

TEST_CASE("API Model") {
    using namespace std::string_literals;

    Options const options{};

    // input data
    DatasetConst input_dataset{"input", 0, 1};

    // node buffer
    std::vector<ID> const node_id{0, 4};
    std::vector<double> const node_u_rated{100.0, 100.0};
    Buffer node_buffer{PGM_def_input_node, 2};
    node_buffer.set_nan(0, node_buffer.size());
    node_buffer.set_value(PGM_def_input_node_id, node_id.data(), -1);
    node_buffer.set_value(PGM_def_input_node_u_rated, node_u_rated.data(), -1);

    std::vector<ID> const line_id{5, 6};
    std::vector<ID> const line_from_node{0, 4};
    std::vector<ID> const line_to_node{4, 0};
    std::vector<Idx> const line_from_status{0, 1};
    std::vector<Idx> const line_to_status{1, 0};
    std::vector<ID> const batch_line_id{5, 6, 5, 6};
    std::vector<ID> const batch_line_from_node{0, 4, 0, 4};
    std::vector<ID> const batch_line_to_node{4, 0, 4, 0};
    std::vector<Idx> const batch_line_from_status{0, 1, 0, 1};
    std::vector<Idx> const batch_line_to_status{1, 0, 1, 0};

    // source buffer
    ID const source_id = 1;
    ID const source_node = 0;
    int8_t const source_status = 1;
    double const source_u_ref = 1.0;
    double const source_sk = 1000.0;
    double const source_rx_ratio = 0.0;
    Buffer source_buffer{PGM_def_input_source, 1};
    source_buffer.set_nan();
    source_buffer.set_value(PGM_def_input_source_id, &source_id, -1);
    source_buffer.set_value(PGM_def_input_source_node, &source_node, 0, sizeof(ID));
    source_buffer.set_value(PGM_def_input_source_status, &source_status, -1);
    source_buffer.set_value(PGM_def_input_source_u_ref, &source_u_ref, -1);
    source_buffer.set_value(PGM_def_input_source_sk, &source_sk, -1);
    source_buffer.set_value(PGM_def_input_source_rx_ratio, &source_rx_ratio, -1);

    // load buffer
    ID load_id = 2;
    ID const load_node = 0;
    int8_t const load_status = 1;
    int8_t const load_type = 2;
    double const load_p_specified = 0.0;
    double const load_q_specified = 500.0;
    Buffer load_buffer{PGM_def_input_sym_load, 1};
    load_buffer.set_value(PGM_def_input_sym_load_id, &load_id, -1);
    load_buffer.set_value(PGM_def_input_sym_load_node, &load_node, -1);
    load_buffer.set_value(PGM_def_input_sym_load_status, &load_status, -1);
    load_buffer.set_value(PGM_def_input_sym_load_type, &load_type, -1);
    load_buffer.set_value(PGM_def_input_sym_load_p_specified, &load_p_specified, -1);
    load_buffer.set_value(PGM_def_input_sym_load_q_specified, &load_q_specified, -1);

    // add buffers - row
    input_dataset.add_buffer("sym_load", 1, 1, nullptr, load_buffer);
    input_dataset.add_buffer("source", 1, 1, nullptr, source_buffer);

    // add buffers - columnar
    input_dataset.add_buffer("node", 2, 2, nullptr, nullptr);
    input_dataset.add_attribute_buffer("node", "id", node_id.data());
    input_dataset.add_attribute_buffer("node", "u_rated", node_u_rated.data());
    input_dataset.add_buffer("line", 2, 2, nullptr, nullptr);
    input_dataset.add_attribute_buffer("line", "id", line_id.data());
    input_dataset.add_attribute_buffer("line", "from_node", line_from_node.data());
    input_dataset.add_attribute_buffer("line", "to_node", line_to_node.data());
    input_dataset.add_attribute_buffer("line", "from_status", line_from_status.data());
    input_dataset.add_attribute_buffer("line", "to_status", line_to_status.data());

    // output data
    Buffer node_output{PGM_def_sym_output_node, 2};
    node_output.set_nan();
    DatasetMutable single_output_dataset{"sym_output", 0, 1};
    single_output_dataset.add_buffer("node", 2, 2, nullptr, node_output);
    Buffer node_batch_output{PGM_def_sym_output_node, 4};
    node_batch_output.set_nan();
    DatasetMutable batch_output_dataset{"sym_output", 1, 2};
    batch_output_dataset.add_buffer("node", 2, 4, nullptr, node_batch_output);

    std::vector<ID> node_result_id(2);
    std::vector<int8_t> node_result_energized(2);
    std::vector<double> node_result_u(2);
    std::vector<double> node_result_u_pu(2);
    std::vector<double> node_result_u_angle(2);
    std::vector<ID> batch_node_result_id(4);
    std::vector<int8_t> batch_node_result_energized(4);
    std::vector<double> batch_node_result_u(4);
    std::vector<double> batch_node_result_u_pu(4);
    std::vector<double> batch_node_result_u_angle(4);

    // update data
    ID source_update_id = 1;
    int8_t const source_update_status = std::numeric_limits<int8_t>::min();
    double const source_update_u_ref = 0.5;
    double const source_update_u_ref_angle = std::numeric_limits<double>::quiet_NaN();
    Buffer source_update_buffer{PGM_def_update_source, 1};
    source_update_buffer.set_nan();
    source_update_buffer.set_value(PGM_def_update_source_id, &source_update_id, 0, -1);
    source_update_buffer.set_value(PGM_def_update_source_status, &source_update_status, 0, -1);
    source_update_buffer.set_value(PGM_def_update_source_u_ref, &source_update_u_ref, 0, -1);
    source_update_buffer.set_value(PGM_def_update_source_u_ref_angle, &source_update_u_ref_angle, 0, -1);
    std::array<Idx, 3> source_update_indptr{0, 1, 1};

    std::vector<ID> load_updates_id = {2, 2};
    std::vector<double> load_updates_q_specified = {100.0, 300.0};
    Buffer load_updates_buffer{PGM_def_update_sym_load, 2};
    // set nan twice with offset
    load_updates_buffer.set_nan(0);
    load_updates_buffer.set_nan(1);
    load_updates_buffer.set_value(PGM_def_update_sym_load_id, load_updates_id.data(), -1);
    load_updates_buffer.set_value(PGM_def_update_sym_load_q_specified, load_updates_q_specified.data(), 0, -1);
    load_updates_buffer.set_value(PGM_def_update_sym_load_q_specified, load_updates_q_specified.data(), 1, -1);
    // dataset
    DatasetConst single_update_dataset{"update", 0, 1};
    single_update_dataset.add_buffer("source", 1, 1, nullptr, source_update_buffer);
    single_update_dataset.add_buffer("sym_load", 1, 1, nullptr, load_updates_buffer.get());
    single_update_dataset.add_buffer("line", 2, 2, nullptr, nullptr);
    single_update_dataset.add_attribute_buffer("line", "id", line_id.data());
    single_update_dataset.add_attribute_buffer("line", "from_status", line_from_status.data());
    single_update_dataset.add_attribute_buffer("line", "to_status", line_to_status.data());
    DatasetConst batch_update_dataset{"update", 1, 2};
    batch_update_dataset.add_buffer("source", -1, 1, source_update_indptr.data(), source_update_buffer.get());
    batch_update_dataset.add_buffer("sym_load", 1, 2, nullptr, load_updates_buffer);
    batch_update_dataset.add_buffer("line", 2, 4, nullptr, nullptr);
    batch_update_dataset.add_attribute_buffer("line", "id", batch_line_id.data());
    batch_update_dataset.add_attribute_buffer("line", "from_status", batch_line_from_status.data());
    batch_update_dataset.add_attribute_buffer("line", "to_status", batch_line_to_status.data());

    // create model
    Model model{50.0, input_dataset};

    // test move-ability
    Model model_dummy{std::move(model)};
    model = std::move(model_dummy);

    SUBCASE("Simple power flow") {
        model.calculate(options, single_output_dataset);
        node_output.get_value(PGM_def_sym_output_node_id, node_result_id.data(), -1);
        node_output.get_value(PGM_def_sym_output_node_energized, node_result_energized.data(), 0, -1);
        node_output.get_value(PGM_def_sym_output_node_u, node_result_u.data(), 0, 1, -1);
        node_output.get_value(PGM_def_sym_output_node_u_pu, node_result_u_pu.data(), -1);
        node_output.get_value(PGM_def_sym_output_node_u_angle, node_result_u_angle.data(), -1);
        CHECK(node_result_id[0] == 0);
        CHECK(node_result_energized[0] == 1);
        CHECK(node_result_u[0] == doctest::Approx(50.0));
        CHECK(node_result_u_pu[0] == doctest::Approx(0.5));
        CHECK(node_result_u_angle[0] == doctest::Approx(0.0));
        CHECK(node_result_id[1] == 4);
        CHECK(node_result_energized[1] == 0);
        CHECK(node_result_u[1] == doctest::Approx(0.0));
        CHECK(node_result_u_pu[1] == doctest::Approx(0.0));
        CHECK(node_result_u_angle[1] == doctest::Approx(0.0));
    }

    SUBCASE("Simple update") {
        model.update(single_update_dataset);
        model.calculate(options, single_output_dataset);
        node_output.get_value(PGM_def_sym_output_node_id, node_result_id.data(), -1);
        node_output.get_value(PGM_def_sym_output_node_energized, node_result_energized.data(), 0, -1);
        node_output.get_value(PGM_def_sym_output_node_u, node_result_u.data(), 0, 1, -1);
        node_output.get_value(PGM_def_sym_output_node_u_pu, node_result_u_pu.data(), -1);
        node_output.get_value(PGM_def_sym_output_node_u_angle, node_result_u_angle.data(), -1);
        CHECK(node_result_id[0] == 0);
        CHECK(node_result_energized[0] == 1);
        CHECK(node_result_u[0] == doctest::Approx(40.0));
        CHECK(node_result_u_pu[0] == doctest::Approx(0.4));
        CHECK(node_result_u_angle[0] == doctest::Approx(0.0));
        CHECK(node_result_id[1] == 4);
        CHECK(node_result_energized[1] == 0);
        CHECK(node_result_u[1] == doctest::Approx(0.0));
        CHECK(node_result_u_pu[1] == doctest::Approx(0.0));
        CHECK(node_result_u_angle[1] == doctest::Approx(0.0));
    }

    SUBCASE("Copy model") {
        auto const model_copy = Model{model};
        model_copy.calculate(options, single_output_dataset);
        node_output.get_value(PGM_def_sym_output_node_id, node_result_id.data(), -1);
        node_output.get_value(PGM_def_sym_output_node_energized, node_result_energized.data(), 0, -1);
        node_output.get_value(PGM_def_sym_output_node_u, node_result_u.data(), 0, 1, -1);
        node_output.get_value(PGM_def_sym_output_node_u_pu, node_result_u_pu.data(), -1);
        node_output.get_value(PGM_def_sym_output_node_u_angle, node_result_u_angle.data(), -1);
        CHECK(node_result_id[0] == 0);
        CHECK(node_result_energized[0] == 1);
        CHECK(node_result_u[0] == doctest::Approx(50.0));
        CHECK(node_result_u_pu[0] == doctest::Approx(0.5));
        CHECK(node_result_u_angle[0] == doctest::Approx(0.0));
        CHECK(node_result_id[1] == 4);
        CHECK(node_result_energized[1] == 0);
        CHECK(node_result_u[1] == doctest::Approx(0.0));
        CHECK(node_result_u_pu[1] == doctest::Approx(0.0));
        CHECK(node_result_u_angle[1] == doctest::Approx(0.0));
    }

    SUBCASE("Get indexer") {
        std::array<ID, 2> ids{2, 2};
        std::array<Idx, 2> indexer{3, 3};
        model.get_indexer("sym_load", 2, ids.data(), indexer.data());
        CHECK(indexer[0] == 0);
        CHECK(indexer[1] == 0);
        ids[1] = 6;
        CHECK_THROWS_AS(model.get_indexer("sym_load", 2, ids.data(), indexer.data()), PowerGridRegularError);
    }

    SUBCASE("Batch power flow") {
        model.calculate(options, batch_output_dataset, batch_update_dataset);
        node_batch_output.get_value(PGM_def_sym_output_node_id, batch_node_result_id.data(), -1);
        node_batch_output.get_value(PGM_def_sym_output_node_energized, batch_node_result_energized.data(), -1);
        node_batch_output.get_value(PGM_def_sym_output_node_u, batch_node_result_u.data(), -1);
        node_batch_output.get_value(PGM_def_sym_output_node_u_pu, batch_node_result_u_pu.data(), -1);
        node_batch_output.get_value(PGM_def_sym_output_node_u_angle, batch_node_result_u_angle.data(), -1);
        CHECK(batch_node_result_id[0] == 0);
        CHECK(batch_node_result_energized[0] == 1);
        CHECK(batch_node_result_u[0] == doctest::Approx(40.0));
        CHECK(batch_node_result_u_pu[0] == doctest::Approx(0.4));
        CHECK(batch_node_result_u_angle[0] == doctest::Approx(0.0));
        CHECK(batch_node_result_id[1] == 4);
        CHECK(batch_node_result_energized[1] == 0);
        CHECK(batch_node_result_u[1] == doctest::Approx(0.0));
        CHECK(batch_node_result_u_pu[1] == doctest::Approx(0.0));
        CHECK(batch_node_result_u_angle[1] == doctest::Approx(0.0));
        CHECK(batch_node_result_id[2] == 0);
        CHECK(batch_node_result_energized[2] == 1);
        CHECK(batch_node_result_u[2] == doctest::Approx(70.0));
        CHECK(batch_node_result_u_pu[2] == doctest::Approx(0.7));
        CHECK(batch_node_result_u_angle[2] == doctest::Approx(0.0));
        CHECK(batch_node_result_id[3] == 4);
        CHECK(batch_node_result_energized[3] == 0);
        CHECK(batch_node_result_u[3] == doctest::Approx(0.0));
        CHECK(batch_node_result_u_pu[3] == doctest::Approx(0.0));
        CHECK(batch_node_result_u_angle[3] == doctest::Approx(0.0));
    }

    SUBCASE("Input error handling") {
        SUBCASE("Construction error") {
            load_id = 0;
            load_buffer.set_value(PGM_def_input_sym_load_id, &load_id, -1);
            source_update_id = 1;
            source_update_buffer.set_value(PGM_def_update_source_id, &source_update_id, 0, -1);
            auto const wrong_model_lambda = [&input_dataset]() { Model const wrong_model{50.0, input_dataset}; };
            check_throws_with(wrong_model_lambda, PGM_regular_error, "Conflicting id detected:"s);
        }

        SUBCASE("Update error") {
            load_id = 2;
            load_buffer.set_value(PGM_def_input_sym_load_id, &load_id, -1);
            source_update_id = 99;
            source_update_buffer.set_value(PGM_def_update_source_id, &source_update_id, 0, -1);
            auto const bad_update_lambda = [&model, &single_update_dataset]() { model.update(single_update_dataset); };
            check_throws_with(bad_update_lambda, PGM_regular_error, "The id cannot be found:"s);
        }

        SUBCASE("Invalid calculation type error") {
            auto const bad_calc_type_lambda = [&options, &model, &single_output_dataset]() {
                options.set_calculation_type(-128);
                model.calculate(options, single_output_dataset);
            };
            check_throws_with(bad_calc_type_lambda, PGM_regular_error, "CalculationType is not implemented for"s);
        }

        SUBCASE("Invalid tap changing strategy error") {
            auto const bad_tap_strat_lambda = [&options, &model, &single_output_dataset]() {
                options.set_tap_changing_strategy(-128);
                model.calculate(options, single_output_dataset);
            };
            check_throws_with(bad_tap_strat_lambda, PGM_regular_error, "get_optimizer_type is not implemented for"s);
        }

        SUBCASE("Tap changing strategy") {
            options.set_tap_changing_strategy(PGM_tap_changing_strategy_min_voltage_tap);
            CHECK_NOTHROW(model.calculate(options, single_output_dataset));
        }
    }

    SUBCASE("Calculation error") {
        SUBCASE("Single calculation error") {
            // not converging
            options.set_max_iter(1);
            options.set_err_tol(1e-100);
            options.set_symmetric(0);
            options.set_threading(1);
            auto const calc_error_lambda = [&model, &single_output_dataset](auto const& opt) {
                model.calculate(opt, single_output_dataset);
            };
            check_throws_with(calc_error_lambda, PGM_regular_error, "Iteration failed to converge after"s, options);

            // wrong method
            options.set_calculation_type(PGM_state_estimation);
            options.set_calculation_method(PGM_iterative_current);
            check_throws_with(calc_error_lambda, PGM_regular_error,
                              "The calculation method is invalid for this calculation!"s, options);
        }

        SUBCASE("Batch calculation error") {
            // wrong id
            load_updates_id[1] = 999;
            load_updates_buffer.set_value(PGM_def_update_sym_load_id, load_updates_id.data(), 1, -1);
            // failed in batch 1
            try {
                model.calculate(options, batch_output_dataset, batch_update_dataset);
                FAIL("Expected batch calculation error not thrown.");
            } catch (PowerGridBatchError const& e) {
                CHECK(e.error_code() == PGM_batch_error);
                auto const& failed_scenarios = e.failed_scenarios();
                CHECK(failed_scenarios.size() == 1);
                CHECK(failed_scenarios[0].scenario == 1);
                std::string const err_msg{failed_scenarios[0].error_message};
                CHECK(err_msg.find("The id cannot be found:"s) != std::string::npos);
            }
            // valid results for batch 0
            node_batch_output.get_value(PGM_def_sym_output_node_id, batch_node_result_id.data(), -1);
            node_batch_output.get_value(PGM_def_sym_output_node_energized, batch_node_result_energized.data(), -1);
            node_batch_output.get_value(PGM_def_sym_output_node_u, batch_node_result_u.data(), -1);
            node_batch_output.get_value(PGM_def_sym_output_node_u_pu, batch_node_result_u_pu.data(), -1);
            node_batch_output.get_value(PGM_def_sym_output_node_u_angle, batch_node_result_u_angle.data(), -1);
            CHECK(batch_node_result_id[0] == 0);
            CHECK(batch_node_result_energized[0] == 1);
            CHECK(batch_node_result_u[0] == doctest::Approx(40.0));
            CHECK(batch_node_result_u_pu[0] == doctest::Approx(0.4));
            CHECK(batch_node_result_u_angle[0] == doctest::Approx(0.0));
            CHECK(batch_node_result_id[1] == 4);
            CHECK(batch_node_result_energized[1] == 0);
            CHECK(batch_node_result_u[1] == doctest::Approx(0.0));
            CHECK(batch_node_result_u_pu[1] == doctest::Approx(0.0));
            CHECK(batch_node_result_u_angle[1] == doctest::Approx(0.0));
        }
    }
}

} // namespace power_grid_model_cpp
