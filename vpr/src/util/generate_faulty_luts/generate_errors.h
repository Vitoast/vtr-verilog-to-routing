//
// Created by Tobias Steinbach
//
// This file is used to generate a fault-LUT-cell file from the current architecture.
// In the file for each cell in a LUT a code is saved. (0: Fault-Free, 1: Stuck-At-1, 2:Stuck-At-0, 3:Stuck-At-Undefined)
// The cell of the CLBs are seperated in lines.
//

#ifndef VTR_VERILOG_TO_ROUTING_GENERATE_ERRORS_H
#define VTR_VERILOG_TO_ROUTING_GENERATE_ERRORS_H

#include <iostream>
using std::cerr;
using std::endl;
#include <fstream>
using std::ofstream;
#include <cstdlib>
#include <random>

#include "vpr_utils.h"
#include "vpr_context.h"
#include "physical_types.h"
#include "device_grid.h"
#include "read_options.h"
#include "globals.h"
#include "vtr_log.h"

/*
 * This is used to generate the error file by given probabilities for each fault-type.
 */
void generate_device_faults(double sa0,
                            double sa1,
                            double sau);


#endif //VTR_VERILOG_TO_ROUTING_GENERATE_ERRORS_H
