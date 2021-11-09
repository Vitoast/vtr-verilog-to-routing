#ifndef VPR_INITIAL_PLACEMENT_H
#define VPR_INITIAL_PLACEMENT_H

#include "vpr_types.h"
#include "place_constraints.h"

void initial_placement(enum e_pad_loc_type pad_loc_type, const char* constraints_file, std::map<AtomBlockId, Change_Entry>* final_perms, char** lut_errors);

#endif
