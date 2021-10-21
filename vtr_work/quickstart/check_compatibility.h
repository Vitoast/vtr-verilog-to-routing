//
// Created by Tobias Steinbach
//

#ifndef VTR_VERILOG_TO_ROUTING_CHECK_COMPATIBILITY_H
#define VTR_VERILOG_TO_ROUTING_CHECK_COMPATIBILITY_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "vtr_log.h"
#include "vpr_context.h"
#include "globals.h"

using std::ofstream;
using std::ifstream;

/*
 * Writes the computed compatibility information of CLBs and packed functions to a file.
 */
void create_compatibility_matrix();

/*
 * Writes the compatibility information for one CLB into the file.
 * Uses Bit-Stuffing to compress the resulting file.
 * If a CLB can instantiate all functions,
 * then in place of the compatibility information a placeholder in form of an all-1-Byte is stored.
 * To prevent misinterpreting an ordinary CLB-compatibility-row if the first 8 bits would be set,
 * after the 7th bit a 0 is inserted.
 * This must be taken into account when reading the file.
 * If the information stops before the last Byte is filled, a new Byte is started for the next CLB anyway.
 */
void print_bits_to_file(std::vector<bool> result, int num_clbs, std::ofstream& comp_data, bool* compressable);

#endif //VTR_VERILOG_TO_ROUTING_CHECK_COMPATIBILITY_H
