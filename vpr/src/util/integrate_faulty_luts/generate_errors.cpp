//
// Created by Tobias Steinbach
//

#include "generate_errors.h"


void generate_device_faults(double sa0,
                            double sa1,
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

        //get the necessary device information
        auto& dev_ctx = g_vpr_ctx.device();
        int num_luts_per_clb = 0;
        size_t num_clbs = 0;
        //search for block type "clb"
        for (i = 0; i < dev_ctx.num_block_types; ++i) {
            auto type = &dev_ctx.block_types[i];
            //if "clb" found, get information
            if (strcmp(type->name, "clb") == 0) {
                num_clbs = dev_ctx.grid.num_instances(type);
                //iterate over all modes of the CLB and extract number of included logic parts (LUTs)
                //use maximum number of parts in modes
                for (int j = 0; j < type->pb_type->num_modes; ++j) {
                    for (int k = 0; k < type->pb_type->modes[j].num_pb_type_children; ++k) {
                        if (type->pb_type->modes[j].pb_type_children[k].num_pb > num_luts_per_clb) {
                            num_luts_per_clb = type->pb_type->modes[j].pb_type_children[k].num_pb;
                        }
                    }
                }
                break;
            }
        }
        //set LUT size to default 6
        int lut_size = 6;

        //print header to file
        data << "This represents fault types of Bits of LUTs of CLBs (0: FF, 1: SA1, 2:SA0, 3:SAU). Number of CLBs and LUTs per CLB:" << endl;
        data << num_clbs << endl;
        data << num_luts_per_clb << endl;

        //generate errors for each bit in luts by given probabilities
        for (i = 0; i < static_cast<int>(num_clbs); ++i) {
            for (int j = 0; j < num_luts_per_clb * pow(2, lut_size); ++j) {
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

    data.close();
}