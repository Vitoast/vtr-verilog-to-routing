//
// Created by Tobias Steinbach
//
// This file is used to generate a fault-LUT-cell file from the current architecture.
// In the file for each cell in a LUT a code is saved. (0: Fault-Free, 1: Stuck-At-1, 2:Stuck-At-0, 3:Stuck-At-Undefined)
// The cell of the CLBs are seperated in lines.
//

#ifndef VTR_VERILOG_TO_ROUTING_GENERATE_ERRORS_H
#define VTR_VERILOG_TO_ROUTING_GENERATE_ERRORS_H

/*
 * This is used to generate the LUT-error file by given probabilities for each fault-type.
 */
void generate_device_faults(double sa0,
                            double sa1,
                            double sau);

void read_lut_error_information(char*** lut_errors, int* num_clbs);

#endif //VTR_VERILOG_TO_ROUTING_GENERATE_ERRORS_H
