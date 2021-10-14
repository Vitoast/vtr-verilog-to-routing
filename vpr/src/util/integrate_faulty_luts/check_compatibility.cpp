//
// Created by Tobias Steinbach
//

#include "check_compatibility.h"

std::vector<bool> check_compatibility_clb(char* clb, int num_clbs, int luts_per_clb, bool* compressable) {
    int num_fcts = 0;
    std::vector<bool> results;

    return results;
}

void print_bits_to_file(std::vector<bool> result, int num_clbs, ofstream& comp_data, bool* compressable) {
    char write = 0;
    //if the whole line is true, a compress-symbol is inserted
    if(compressable) {
        write = -1;
        comp_data << write;
    }
    //otherwise, the result is written bitwise
    else {
        bool stuff = true;
        int write_counter = 0;
        for (int r = 0; r < num_clbs; ++r) {
            if(r < 6) {
                //not compatible, bit on position r remains 0
                if (!result[r]) {
                    //bit stuffing is not necessary if a 0 occurs in the first 7 compatibilities
                    stuff = false;
                    ++write_counter;
                }
                //compatible, set bit at position r
                else {
                    write |= (0x1 >> r);
                    ++write_counter;
                }
            }
            else {
                //stuff a 0 bit (skip one position in write char)
                if (r == 6 && stuff) {
                    ++write_counter;
                }
                //fill in next compatibility information
                else {
                    if (result[r])
                        write &= !(0x1 >> ((r + (stuff ? 1 : 0)) & 0b00000111));
                    ++write_counter;
                }
            }
            //a char of information is full and must be written out
            if (write_counter == 8) {
                comp_data << write;
                write_counter = 0;
            }
        }
    }
}

void create_compatibility_matrix() {

    ofstream comp_data;
    //open or create comp file
    comp_data.open("file.comp_matrix", ofstream::out | ofstream::trunc);
    if(!comp_data) {
        VTR_LOG("Error file for architecture could not be opened.\n");
    }
    else {
        //stuff for reading error file
        ifstream error_data("device_faults.txt");
        char cur_cell;
        int num_luts_per_clb = 0;
        int num_clbs = 0;

        //read general clb infos from file
        std::string line;
        //read generic header
        std::getline(error_data, line);
        //read number of clbs in second line
        std::getline(error_data, line);
        num_clbs = stoi(line);
        // read number of luts per clb form third line
        std::getline(error_data, line);
        num_luts_per_clb = stoi(line);
        //saves errors for current clb
        char* clb = (char *) malloc(sizeof(char)*num_luts_per_clb*64);
        //read errors for one clb and process it
        for (int i = 0; i < num_clbs; ++i) {
            while (std::getline(error_data, line)) {
                std::istringstream str(line);
                //read clb char/cell by char/cell
                int count_cells = 0;
                int count_luts = 0;
                while(!(str >> cur_cell)) {
                    clb[count_luts * 64 + count_cells] = cur_cell;
                    if(count_cells == 63) {
                        count_cells = 0;
                        ++count_luts;
                    }
                }
                bool compressable = true;
                print_bits_to_file(check_compatibility_clb(clb, num_clbs, num_luts_per_clb, &compressable), num_clbs, comp_data, &compressable);
            }
        }
        //free boolean array
        free(clb);
    }
}


