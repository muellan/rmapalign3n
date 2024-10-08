/******************************************************************************
 *
 * RmapAlign3N - 3N Read Mapping and Alignment Tool
 *
 * Copyright (C) 2024 André Müller (muellan@uni-mainz.de)
 *                       
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#ifndef RMA_PRINT_RESULTS_H_
#define RMA_PRINT_RESULTS_H_

#include <string>
#include <iosfwd>

#include "config.h"
// #include "classify_common.h"
#include "classification_statistics.h"
#include "matches_per_target.h"


namespace mc {

// forward declaration
class database;
class classification_output_formatting;
class classification_results;


/*************************************************************************//**
 *
 * @brief prints classification parameters
 *
 *****************************************************************************/
void show_query_parameters(std::ostream&, const database& db, const query_options&);


/*************************************************************************//**
 *
 * @brief prints target information according to output options
 *
 *****************************************************************************/
void show_target(std::ostream&,
                const database&,
                const classification_output_formatting&,
                target_id);


/*************************************************************************//**
 *
 * @brief prints header for target information
 *
 *****************************************************************************/
void show_target_header(std::ostream&,
                       const classification_output_formatting&,
                       const std::string& prefix = "");


/*************************************************************************//**
 *
 * @brief prints top classification candidates
 *
 *****************************************************************************/
void show_candidates(std::ostream&,
                     const database&,
                     const classification_candidates&);


/*************************************************************************//**
 *
 * @brief prints target.window hit matches
 *
 *****************************************************************************/
template<class Locations>
void show_matches(std::ostream&,
                  const database&,
                  const Locations&);


/*************************************************************************//**
 *
 * @brief prints target.window hit statistics from database
 *
 *****************************************************************************/
void show_candidate_ranges(std::ostream&,
                           const database&,
                           const classification_candidates&);


/*************************************************************************//**
 *
 * @brief prints mapping statistics
 *
 *****************************************************************************/
void show_statistics(std::ostream&,
                     const mapping_statistics&,
                     const std::string& prefix = "");


/*************************************************************************//**
 *
 * @brief show summary and statistics of classification
 *
 *****************************************************************************/
void show_summary(const query_options& opt,
                  const classification_results& results);


/*************************************************************************//**
 *
 * @brief prints database properties to stdout
 *
 *****************************************************************************/
void print_static_properties(const database&);


/*************************************************************************//**
 *
 * @brief prints database properties to stdout
 *
 *****************************************************************************/
void print_content_properties(const database&);


} // namespace mc


#endif
