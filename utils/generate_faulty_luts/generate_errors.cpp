//
// Created by tobiass on 11.10.21.
//

#include <iostream>
using std::cerr;
using std::endl;
#include <fstream>
using std::ofstream;
#include <cstdlib>
#include <random>

int main();

void generate_device_faults(int num_luts,
                            double sa0,
                            double sa1,
                            double sau);


int main() {
    generate_device_faults(8000, 0.01, 0.01, 0.01);
}

void generate_device_faults(int num_luts,
                            double sa0,
                            double sa1,
                            double sau) {
    ofstream data;
    int i;
    double r;

    data.open("lut_errors.txt");
    if( !data ) {
        cerr << "File could not be opened" << endl;
        exit(1);
    }

    else {
        std::random_device randy;
        std::default_random_engine eng(randy());
        std::uniform_real_distribution<double> distr(0, 1);
        r = distr(eng);

        data << "This represents fault types of " << num_luts << " LUTs (0: FF, 1: SA1, 2:SA0, 3:SAU)" << endl;

        for (int i = 0; i < num_luts * 64; ++i) {
            if (r <= sa1) {
                data << 1;
                r = distr(eng);
                continue;
            }
            else {
                r = distr(eng);
                if(r <= sa0) {
                    data << 2;
                    r = distr(eng);
                }

                else {
                    r = distr(eng);
                    if(r <= sau) {
                        data << 3;
                        r = distr(eng);
                    }
                    else {
                        data << 0;
                        r = distr(eng);
            }}}
        }
    }
}