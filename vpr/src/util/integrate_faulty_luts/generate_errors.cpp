//
// Author: Vincent Tobias Steinbach
//
//Modified: added file to generate errors for a device and read an error-file to use its data while placement.
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <random>
#include <vector>

using std::ofstream;
using std::endl;
using namespace std;

#include "vpr_utils.h"
#include "vpr_context.h"
#include "physical_types.h"
#include "device_grid.h"
#include "read_options.h"
#include "globals.h"
#include "vtr_log.h"
#include "generate_errors.h"


void generate_device_faults(double sa1,
                            double sa0,
                            double sau) {
    ofstream data;
    int i;
    double r;

    //open or create error file
    data.open("device_faults.txt", ofstream::out | ofstream::trunc);
    if(!data) {
        VTR_LOG("Error file for architecture could not be created.\n");
    }

    else {
        //init random number generator
        std::random_device randy;
        std::default_random_engine eng(randy());
        std::uniform_real_distribution<double> distr(0, 1);
        r = distr(eng);
        //get device and clb information
        auto& device_ctx = g_vpr_ctx.device();
        size_t num_blocks = device_ctx.grid.matrix().size();
        get_clb_information();

        //print header to file
        data << "This represents fault types of Bits of LUTs of CLBs (0: FF, 1: SA1, 2:SA0, 3:SAU). Number of blocks in the grid and LUTs per CLB:" << endl;
        data << num_blocks << endl;
        data << device_ctx.num_luts_per_clb << endl;

        //generate errors for each bit in luts by given probabilities
        for (i = 0; i < static_cast<int>(num_blocks); ++i) {
            for (int j = 0; j < device_ctx.num_luts_per_clb * pow(2, device_ctx.lut_size); ++j) {
                //add SA1 error
                if (r <= sa1) {
                    data << 1;
                    r = distr(eng);
                    continue;
                }
                else {
                    r = distr(eng);
                    //add SA0 error
                    if(r <= sa0) {
                        data << 2;
                        r = distr(eng);
                    }
                    else {
                        r = distr(eng);
                        //add SAU error
                        if(r <= sau) {
                            data << 3;
                            r = distr(eng);
                        }
                        //add fault-free cell
                        else {
                            data << 0;
                            r = distr(eng);
                        }
                    }
                }

            }
            //separate CLBs
            data << endl;
        }
    }
    VTR_LOG("# File with random faults in LUT-memory-cells was created.\n");
    data.close();
}

void get_clb_information() {
    //get the necessary device information
    auto& device_ctx = g_vpr_ctx.mutable_device();
    int num_luts_per_clb = 0;
    int lut_size = 0;
    //iterate over all block types in the device to search for CLBs
    for (const auto& log_type : device_ctx.logical_block_types) {
        const auto& type = log_type.pb_type;
        //if type clb found, get information
        if(pb_type_contains_blif_model(type, ".names")) {
            //iterate over all modes of the CLB and extract number of included logic parts (LUTs)
            //use maximum number of parts in modes
            for (int j = 0; j < type->num_modes; ++j) {
                for (int k = 0; k < type->modes[j].num_pb_type_children; ++k) {
                    if (type->modes[j].pb_type_children[k].num_pb > num_luts_per_clb) {
                        num_luts_per_clb = type->modes[j].pb_type_children[k].num_pb;
                        lut_size = type->modes[j].pb_type_children[k].num_input_pins;
                    }
                }
            }
            break;
        }
    }
    //assign calculated values to fields in device context
    device_ctx.num_luts_per_clb = num_luts_per_clb;
    device_ctx.lut_size = lut_size;

}

void read_lut_error_information(char*** lut_errors, int* num_blocks) {
    //stuff for reading error file
    ifstream error_data("device_faults.txt");
    char cur_cell;
    auto& dev_ctx = g_vpr_ctx.device();
    //if error-file was not generated in this run the clb information must be calculated now
    if(!dev_ctx.generate_errors_random)
        get_clb_information();
    int num_luts_per_clb = 0;
    //there are 2^lutsize cells per lut
    int num_cells_per_lut = (int) pow(2, dev_ctx.lut_size);
    *num_blocks = 0;
    //read general clb infos from file
    std::string line;
    //read generic header
    std::getline(error_data, line);
    //read number of clbs in second line
    std::getline(error_data, line);
    *num_blocks = stoi(line);
    // read number of luts per clb form third line
    std::getline(error_data, line);
    num_luts_per_clb = stoi(line);
    //allocate array for fault data
    *lut_errors = (char **) malloc((*num_blocks) * sizeof(char*));
    //read errors for one clb and process it
    for (int i = 0; i < *num_blocks; ++i) {
        //allocate data for one clb
        (*lut_errors)[i] = (char *) malloc(num_luts_per_clb*num_cells_per_lut * sizeof(char));
        int j = 0;
        std::getline(error_data, line);
        std::istringstream str(line);
        //read clb char/cell by char/cell
        while(str >> cur_cell) {
            (*lut_errors)[i][j] = cur_cell;
            ++j;
        }
    }
    VTR_LOG("# Information about faulty LUT-memory-cells were read in.\n");
}