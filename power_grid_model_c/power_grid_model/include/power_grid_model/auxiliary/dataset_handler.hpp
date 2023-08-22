// SPDX-FileCopyrightText: 2022 Contributors to the Power Grid Model project <dynamic.grid.calculation@alliander.com>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once
#ifndef POWER_GRID_MODEL_AUXILIARY_DATASET_HANDLER_HPP
#define POWER_GRID_MODEL_AUXILIARY_DATASET_HANDLER_HPP

// handle dataset and buffer related stuff

#include "../exception.hpp"
#include "../power_grid_model.hpp"
#include "meta_data.hpp"
#include "meta_data_gen.hpp"

#include <span>
#include <string_view>

namespace power_grid_model::meta_data {

struct ComponentInfo {
    MetaComponent const* component;
    // for non-uniform component, this is -1, we use indptr to describe the elements per scenario
    Idx elements_per_scenario;
    Idx total_elements;
};

struct DatasetDescription {
    bool is_batch;
    Idx batch_size; // for single dataset, the batch size is one
    MetaDataset const* dataset;
    std::vector<ComponentInfo> component_info;
};

template <bool data_mutable, bool indptr_mutable>
    requires(data_mutable || !indptr_mutable)
class DatasetHandler {
  public:
    using Data = std::conditional_t<data_mutable, void, void const>;
    using Indptr = std::conditional_t<indptr_mutable, Idx, Idx const>;
    struct Buffer {
        Data* data;
        // for uniform buffer, indptr is empty
        std::span<Indptr> indptr;
    };

    DatasetHandler(bool is_batch, Idx batch_size, std::string_view dataset)
        : description_{.is_batch = is_batch,
                       .batch_size = batch_size,
                       .dataset = &meta_data().get_dataset(dataset),
                       .component_info = {}} {
        if (!description_.is_batch && (description_.batch_size != 1)) {
            throw DatasetError{"For non-batch dataset, batch size should be one!\n"};
        }
    }

    bool is_batch() const { return description_.is_batch; }
    Idx batch_size() const { return description_.batch_size; }
    MetaDataset const& dataset() const { return *description_.dataset; }
    Idx n_components() const { return static_cast<Idx>(buffers_.size()); }
    DatasetDescription const& get_description() const { return description_; }
    ComponentInfo const& get_component_info(Idx i) const { return description_.component_info[i]; }
    Buffer const& get_buffer(std::string_view component) const { return get_buffer(find_component(component, true)); }
    Buffer const& get_buffer(Idx i) const { return buffers_[i]; }

    Idx find_component(std::string_view component, bool throw_not_found = false) const {
        auto const found = std::find_if(description_.component_info.cbegin(), description_.component_info.cend(),
                                        [component](ComponentInfo const& x) { return x.component->name == component; });
        if (found == description_.component_info.cend()) {
            if (throw_not_found) {
                throw DatasetError{"Cannot find component!\n"};
            } else {
                return -1;
            }
        }
        return std::distance(description_.component_info.cbegin(), found);
    }

    ComponentInfo const& get_component_info(std::string_view component) const {
        return description_.component_info[find_component(component, true)];
    }

    void add_component_info(std::string_view component, Idx elements_per_scenario, Idx total_elements) {
        if (find_component(component) >= 0) {
            throw DatasetError{"Cannot have duplicated components!\n"};
        }
        description_.component_info.push_back(
            {&description_.dataset->get_component(component), elements_per_scenario, total_elements});
        buffers_.push_back(Buffer{});
        check_uniform_integrity(elements_per_scenario, total_elements);
    }

    void add_buffer(std::string_view component, Idx elements_per_scenario, Idx total_elements, Indptr* indptr,
                    Data* data) {
        add_component_info(component, elements_per_scenario, total_elements);
        check_non_uniform_integrity<true>(elements_per_scenario, total_elements, indptr);
        buffers_.back().data = data;
        if (indptr) {
            buffers_.back().indptr = {indptr, static_cast<size_t>(batch_size() + 1)};
        } else {
            buffers_.back().indptr = {};
        }
    }

    void set_buffer(std::string_view component, Indptr* indptr, Data* data) {
        Idx const idx = find_component(component, true);
        ComponentInfo const& info = description_.component_info[idx];
        check_non_uniform_integrity<false>(info.elements_per_scenario, info.total_elements, indptr);
        buffers_[idx].data = data;
        if (indptr) {
            buffers_[idx].indptr = {indptr, static_cast<size_t>(batch_size() + 1)};
        } else {
            buffers_[idx].indptr = {};
        }
    }

  private:
    DatasetDescription description_;
    std::vector<Buffer> buffers_;

    void check_uniform_integrity(Idx elements_per_scenario, Idx total_elements) {
        if ((elements_per_scenario >= 0) && (elements_per_scenario * batch_size() != total_elements)) {
            throw DatasetError{
                "For a uniform buffer, totoal_element should be equal to elements_per_scenario * batch_size !\n"};
        }
    }

    template <bool check_indptr_content>
    void check_non_uniform_integrity(Idx elements_per_scenario, Idx total_elements, Indptr* indptr) {
        if (elements_per_scenario < 0) {
            if (!indptr) {
                throw DatasetError{"For a non-uniform buffer, indptr should be supplied !\n"};
            }
            if constexpr (check_indptr_content) {
                if (indptr[0] != 0 || indptr[batch_size()] != total_elements) {
                    throw DatasetError{
                        "For a non-uniform buffer, indptr should begin with 0 and end with total_elements !\n"};
                }
            }
        } else {
            if (indptr) {
                throw DatasetError{"For a uniform buffer, indptr should be nullptr !\n"};
            }
        }
    }
};

using ConstDatasetHandler = DatasetHandler<false, false>;
using MutableDatasetHandler = DatasetHandler<true, false>;
using WritableDatasetHandler = DatasetHandler<true, true>;

} // namespace power_grid_model::meta_data

#endif