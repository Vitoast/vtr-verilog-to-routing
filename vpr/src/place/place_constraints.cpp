/*
 * place_constraints.cpp
 *
 *  Created on: Mar. 1, 2021
 *      Author: khalid88
 *
 *  This file contains routines that help with making sure floorplanning constraints are respected throughout
 *  the placement stage of VPR.
 */

#include "globals.h"
#include "place_constraints.h"
#include "place_util.h"

/*checks that each block's location is compatible with its floorplanning constraints if it has any
 *
 * Modified: added handover and consideration of LUT error matrix
 * */
int check_placement_floorplanning(char** lut_errors) {
    int error = 0;
    auto& cluster_ctx = g_vpr_ctx.clustering();
    auto& place_ctx = g_vpr_ctx.placement();

    for (auto blk_id : cluster_ctx.clb_nlist.blocks()) {
        auto loc = place_ctx.block_locs[blk_id].loc;
        std::map<AtomBlockId, Change_Entry> map;
        if (!cluster_floorplanning_legal(blk_id, loc, &map, lut_errors)) {
            error++;
            VTR_LOG_ERROR("Block %zu is not in correct floorplanning region.\n", size_t(blk_id));
        }
    }

    return error;
}

/*returns true if cluster has floorplanning constraints, false if it doesn't*/
bool is_cluster_constrained(ClusterBlockId blk_id) {
    auto& floorplanning_ctx = g_vpr_ctx.floorplanning();
    PartitionRegion pr;
    pr = floorplanning_ctx.cluster_constraints[blk_id];
    return (!pr.empty());
}

bool is_macro_constrained(const t_pl_macro& pl_macro) {
    bool is_macro_constrained = false;
    bool is_member_constrained = false;

    for (size_t imember = 0; imember < pl_macro.members.size(); imember++) {
        ClusterBlockId iblk = pl_macro.members[imember].blk_index;
        is_member_constrained = is_cluster_constrained(iblk);

        if (is_member_constrained) {
            is_macro_constrained = true;
            break;
        }
    }

    return is_macro_constrained;
}

/*Returns PartitionRegion of where the head of the macro could go*/
PartitionRegion update_macro_head_pr(const t_pl_macro& pl_macro, const PartitionRegion& grid_pr) {
    PartitionRegion macro_head_pr;
    bool is_member_constrained = false;
    int num_constrained_members = 0;
    auto& floorplanning_ctx = g_vpr_ctx.floorplanning();

    for (size_t imember = 0; imember < pl_macro.members.size(); imember++) {
        ClusterBlockId iblk = pl_macro.members[imember].blk_index;
        is_member_constrained = is_cluster_constrained(iblk);

        if (is_member_constrained) {
            num_constrained_members++;
            //PartitionRegion of the constrained block
            PartitionRegion block_pr;
            //PartitionRegion of the constrained block modified for the head according to the offset
            PartitionRegion modified_pr;

            block_pr = floorplanning_ctx.cluster_constraints[iblk];
            std::vector<Region> block_regions = block_pr.get_partition_region();

            for (unsigned int i = 0; i < block_regions.size(); i++) {
                Region modified_reg;
                auto offset = pl_macro.members[imember].offset;

                vtr::Rect<int> reg_rect = block_regions[i].get_region_rect();

                modified_reg.set_region_rect(reg_rect.xmin() - offset.x, reg_rect.ymin() - offset.y, reg_rect.xmax() - offset.x, reg_rect.ymax() - offset.y);

                //check that subtile is not an invalid value before changing, otherwise it just stays -1
                if (block_regions[i].get_sub_tile() != NO_SUBTILE) {
                    modified_reg.set_sub_tile(block_regions[i].get_sub_tile() - offset.sub_tile);
                }

                modified_pr.add_to_part_region(modified_reg);
            }

            if (num_constrained_members == 1) {
                macro_head_pr = modified_pr;
            } else {
                macro_head_pr = intersection(macro_head_pr, modified_pr);
            }
        }
    }

    //intersect to ensure the head pr does not go outside of grid dimensions
    macro_head_pr = intersection(macro_head_pr, grid_pr);

    //if the intersection is empty, no way to place macro members together, give an error
    if (macro_head_pr.empty()) {
        print_macro_constraint_error(pl_macro);
    }

    return macro_head_pr;
}

PartitionRegion update_macro_member_pr(PartitionRegion& head_pr, const t_pl_offset& offset, const PartitionRegion& grid_pr, const t_pl_macro& pl_macro) {
    std::vector<Region> block_regions = head_pr.get_partition_region();
    PartitionRegion macro_pr;

    for (unsigned int i = 0; i < block_regions.size(); i++) {
        Region modified_reg;

        vtr::Rect<int> reg_rect = block_regions[i].get_region_rect();

        modified_reg.set_region_rect(reg_rect.xmin() + offset.x, reg_rect.ymin() + offset.y, reg_rect.xmax() + offset.x, reg_rect.ymax() + offset.y);

        //check that subtile is not an invalid value before changing, otherwise it just stays -1
        if (block_regions[i].get_sub_tile() != NO_SUBTILE) {
            modified_reg.set_sub_tile(block_regions[i].get_sub_tile() + offset.sub_tile);
        }

        macro_pr.add_to_part_region(modified_reg);
    }

    //intersect to ensure the macro pr does not go outside of grid dimensions
    macro_pr = intersection(macro_pr, grid_pr);

    //if the intersection is empty, no way to place macro members together, give an error
    if (macro_pr.empty()) {
        print_macro_constraint_error(pl_macro);
    }

    return macro_pr;
}

void print_macro_constraint_error(const t_pl_macro& pl_macro) {
    auto& cluster_ctx = g_vpr_ctx.clustering();
    VTR_LOG(
        "Feasible floorplanning constraints could not be calculated for the placement macro. \n"
        "The placement macro contains the following blocks: \n");
    for (unsigned int i = 0; i < pl_macro.members.size(); i++) {
        std::string blk_name = cluster_ctx.clb_nlist.block_name((pl_macro.members[i].blk_index));
        VTR_LOG("Block %s (#%zu) ", blk_name.c_str(), size_t(pl_macro.members[i].blk_index));
    }
    VTR_LOG("\n");
    VPR_ERROR(VPR_ERROR_PLACE, " \n Check that the above-mentioned placement macro blocks have compatible floorplan constraints.\n");
}

void propagate_place_constraints() {
    auto& place_ctx = g_vpr_ctx.placement();
    auto& floorplanning_ctx = g_vpr_ctx.mutable_floorplanning();
    auto& device_ctx = g_vpr_ctx.device();

    //Create a PartitionRegion with grid dimensions
    //Will be used to check that updated PartitionRegions are within grid bounds
    int width = device_ctx.grid.width() - 1;
    int height = device_ctx.grid.height() - 1;
    Region grid_reg;
    grid_reg.set_region_rect(0, 0, width, height);
    PartitionRegion grid_pr;
    grid_pr.add_to_part_region(grid_reg);

    for (auto pl_macro : place_ctx.pl_macros) {
        if (is_macro_constrained(pl_macro)) {
            /*
             * Get the PartitionRegion for the head of the macro
             * based on the constraints of all blocks contained in the macro
             */
            PartitionRegion macro_head_pr = update_macro_head_pr(pl_macro, grid_pr);

            //Update PartitionRegions of all members of the macro
            for (size_t imember = 0; imember < pl_macro.members.size(); imember++) {
                ClusterBlockId iblk = pl_macro.members[imember].blk_index;
                auto offset = pl_macro.members[imember].offset;

                if (imember == 0) { //Update head PR
                    floorplanning_ctx.cluster_constraints[iblk] = macro_head_pr;
                } else { //Update macro member PR
                    PartitionRegion macro_pr = update_macro_member_pr(macro_head_pr, offset, grid_pr, pl_macro);
                    floorplanning_ctx.cluster_constraints[iblk] = macro_pr;
                }
            }
        }
    }
}

/*returns true if location is compatible with floorplanning constraints, false if not*/
/*
 * Even if the block passed in is from a macro, it will work because of the constraints
 * propagation that was done during initial placement.
 *
 * Modified: added handover of LUT error matrix
 */
bool cluster_floorplanning_legal(ClusterBlockId blk_id, const t_pl_loc& loc, std::map<AtomBlockId, Change_Entry>* map, char** lut_errors) {
    auto& floorplanning_ctx = g_vpr_ctx.floorplanning();

    bool floorplanning_good = false;

    bool cluster_constrained = is_cluster_constrained(blk_id);

    //not constrained so will not have floorplanning issues
    if (!cluster_constrained) {
        //if block is empty it is always compatible
        if(blk_id == EMPTY_BLOCK_ID) {
            map->clear();
            floorplanning_good = true;
        }
        else { //we need compatibility check only if flag for consideration is set
            if (g_vpr_ctx.placement().consider_faulty_luts) {
                floorplanning_good = check_compatibility_clb(map, lut_errors, blk_id, loc);
            } else {
                floorplanning_good = true;
            }
        }
    } else {
        PartitionRegion pr = floorplanning_ctx.cluster_constraints[blk_id];
        bool in_pr = pr.is_loc_in_part_reg(loc);

        //if location is in partitionregion, floorplanning is respected
        //if not it is not
        if (in_pr) {
            //if block is empty it is always compatible
            if(blk_id == EMPTY_BLOCK_ID) {
                map->clear();
                floorplanning_good = true;
            }
            else {
                //we need compatibility check only if flag for consideration is set
                if (g_vpr_ctx.placement().consider_faulty_luts) {
                    floorplanning_good = check_compatibility_clb(map, lut_errors, blk_id, loc);
                } else {
                    floorplanning_good = true;
                }
            }
        } else {
#ifdef VERBOSE
            VTR_LOG("Block %zu did not pass cluster_floorplanning_check \n", size_t(blk_id));
            VTR_LOG("Loc is x: %d, y: %d, subtile: %d \n", loc.x, loc.y, loc.sub_tile);
#endif
        }
    }

    return floorplanning_good;
}

void load_cluster_constraints() {
    auto& floorplanning_ctx = g_vpr_ctx.mutable_floorplanning();
    auto& cluster_ctx = g_vpr_ctx.clustering();
    ClusterAtomsLookup atoms_lookup;

    floorplanning_ctx.cluster_constraints.resize(cluster_ctx.clb_nlist.blocks().size());

    for (auto cluster_id : cluster_ctx.clb_nlist.blocks()) {
        std::vector<AtomBlockId> atoms = atoms_lookup.atoms_in_cluster(cluster_id);
        PartitionRegion empty_pr;
        floorplanning_ctx.cluster_constraints[cluster_id] = empty_pr;

        //if there are any constrainted atoms in the cluster,
        //we update the cluster's PartitionRegion
        for (unsigned int i = 0; i < atoms.size(); i++) {
            PartitionId partid = floorplanning_ctx.constraints.get_atom_partition(atoms[i]);

            if (partid != PartitionId::INVALID()) {
                PartitionRegion pr = floorplanning_ctx.constraints.get_partition_pr(partid);
                if (floorplanning_ctx.cluster_constraints[cluster_id].empty()) {
                    floorplanning_ctx.cluster_constraints[cluster_id] = pr;
                } else {
                    PartitionRegion intersect_pr = intersection(pr, floorplanning_ctx.cluster_constraints[cluster_id]);
                    if (intersect_pr.empty()) {
                        VTR_LOG_ERROR("Cluster block %zu has atoms with incompatible floorplan constraints.\n", size_t(cluster_id));
                    } else {
                        floorplanning_ctx.cluster_constraints[cluster_id] = intersect_pr;
                    }
                }
            }
        }
    }
}

void mark_fixed_blocks() {
    auto& cluster_ctx = g_vpr_ctx.clustering();
    auto& place_ctx = g_vpr_ctx.mutable_placement();
    auto& floorplanning_ctx = g_vpr_ctx.floorplanning();

    for (auto blk_id : cluster_ctx.clb_nlist.blocks()) {
        if (!is_cluster_constrained(blk_id)) {
            continue;
        }
        PartitionRegion pr = floorplanning_ctx.cluster_constraints[blk_id];
        auto block_type = cluster_ctx.clb_nlist.block_type(blk_id);
        t_pl_loc loc;

        /*
         * If the block can be placed in exactly one
         * legal (x, y, subtile) location, place it now
         * and mark it as fixed.
         */
        if (is_pr_size_one(pr, block_type, loc)) {
            set_block_location(blk_id, loc);

            place_ctx.block_locs[blk_id].is_fixed = true;
        }
    }
}

/*
 * Returns 0, 1, or 2 depending on the number of tiles covered.
 * Will not return a value above 2 because as soon as num_tiles is above 1,
 * it is known that the block that is assigned to this region will not be fixed, and so
 * num_tiles is immediately returned.
 * Updates the location passed in because if num_tiles turns out to be 1 after checking the
 * region, the location that was set will be used as the location to which the block
 * will be fixed.
 */
int region_tile_cover(const Region& reg, t_logical_block_type_ptr block_type, t_pl_loc& loc) {
    auto& device_ctx = g_vpr_ctx.device();
    vtr::Rect<int> rb = reg.get_region_rect();
    int num_tiles = 0;

    for (int x = rb.xmin(); x <= rb.xmax(); x++) {
        for (int y = rb.ymin(); y <= rb.ymax(); y++) {
            auto& tile = device_ctx.grid[x][y].type;

            /*
             * If the tile at the grid location is not compatible with the cluster block
             * type, do not count this tile for num_tiles
             */
            if (!is_tile_compatible(tile, block_type)) {
                continue;
            }

            /*
             * If the region passed has a specific subtile set, increment
             * the number of tiles set the location using the x, y, subtile
             * values if the subtile is compatible at this location
             */
            if (reg.get_sub_tile() != NO_SUBTILE) {
                if (is_sub_tile_compatible(tile, block_type, reg.get_sub_tile())) {
                    num_tiles++;
                    loc.x = x;
                    loc.y = y;
                    loc.sub_tile = reg.get_sub_tile();
                    if (num_tiles > 1) {
                        return num_tiles;
                    }
                }

                /*
                 * If the region passed in does not have a subtile set, set the
                 * subtile to the first possible slot found at this location.
                 */
            } else if (reg.get_sub_tile() == NO_SUBTILE) {
                int num_compatible_st = 0;

                for (int z = 0; z < tile->capacity; z++) {
                    if (is_sub_tile_compatible(tile, block_type, z)) {
                        num_tiles++;
                        num_compatible_st++;
                        if (num_compatible_st == 1) { //set loc.sub_tile to the first compatible subtile value found
                            loc.x = x;
                            loc.y = y;
                            loc.sub_tile = z;
                        }
                        if (num_tiles > 1) {
                            return num_tiles;
                        }
                    }
                }
            }
        }
    }

    return num_tiles;
}

/*
 * Used when marking fixed blocks to check whether the ParitionRegion associated with a block
 * covers one tile. If it covers one tile, it is marked as fixed. If it covers 0 tiles or
 * more than one tile, it will not be marked as fixed. As soon as it is known that the
 * PartitionRegion covers more than one tile, there is no need to check further regions
 * and the routine will return false.
 */
bool is_pr_size_one(PartitionRegion& pr, t_logical_block_type_ptr block_type, t_pl_loc& loc) {
    auto& device_ctx = g_vpr_ctx.device();
    std::vector<Region> regions = pr.get_partition_region();
    bool pr_size_one;
    int pr_size = 0;
    int reg_size;

    Region intersect_reg;
    intersect_reg.set_region_rect(0, 0, device_ctx.grid.width() - 1, device_ctx.grid.height() - 1);
    Region current_reg;

    for (unsigned int i = 0; i < regions.size(); i++) {
        reg_size = region_tile_cover(regions[i], block_type, loc);

        /*
         * If multiple regions in the PartitionRegion all have size 1,
         * the block may still be marked as locked, in the case that
         * they all cover the exact same x, y, subtile location. To check whether this
         * is the case, whenever there is a size 1 region, it is intersected
         * with the previous size 1 regions to see whether it covers the same location.
         * If there is an intersection, it does cover the same location, and so pr_size is
         * not incremented (unless this is the first size 1 region encountered).
         */
        if (reg_size == 1) {
            //get the exact x, y, subtile location covered by the current region (regions[i])
            current_reg.set_region_rect(loc.x, loc.y, loc.x, loc.y);
            current_reg.set_sub_tile(loc.sub_tile);
            intersect_reg = intersection(intersect_reg, current_reg);

            if (i == 0 || intersect_reg.empty()) {
                pr_size = pr_size + reg_size;
                if (pr_size > 1) {
                    break;
                }
            }
        } else {
            pr_size = pr_size + reg_size;
            if (pr_size > 1) {
                break;
            }
        }
    }

    if (pr_size == 1) {
        pr_size_one = true;
    } else { //pr_size = 0 or pr_size > 1
        pr_size_one = false;
    }

    return pr_size_one;
}

int get_part_reg_size(PartitionRegion& pr, t_logical_block_type_ptr block_type, GridTileLookup& grid_tiles) {
    std::vector<Region> part_reg = pr.get_partition_region();
    int num_tiles = 0;

    for (unsigned int i_reg = 0; i_reg < part_reg.size(); i_reg++) {
        num_tiles += grid_tiles.region_tile_count(part_reg[i_reg], block_type);
    }

    return num_tiles;
}

int get_floorplan_score(ClusterBlockId blk_id, PartitionRegion& pr, t_logical_block_type_ptr block_type, GridTileLookup& grid_tiles) {
    auto& cluster_ctx = g_vpr_ctx.clustering();

    int num_pr_tiles = get_part_reg_size(pr, block_type, grid_tiles);

    if (num_pr_tiles == 0) {
        VPR_FATAL_ERROR(VPR_ERROR_PLACE,
                        "Initial placement failed.\n"
                        "The specified floorplan region for block %s (# %d) has no available locations for its type. \n"
                        "Please specify a different floorplan region for the block. Note that if the region has a specified subtile, "
                        "an incompatible subtile location may be the cause of the floorplan region failure. \n",
                        cluster_ctx.clb_nlist.block_name(blk_id).c_str(), blk_id);
    }

    int total_type_tiles = grid_tiles.total_type_tiles(block_type);

    return total_type_tiles - num_pr_tiles;
}

//Modified: added compatibility check between a packed cluster and a physical clb.
//Precondition: clb must not be empty block
bool check_compatibility_clb(std::map<AtomBlockId, Change_Entry>* map, char** lut_errors, ClusterBlockId blk_id, const t_pl_loc& loc) {
    //clear map to avoid old values
    (*map).clear();
    //map to save all possible locations for functions in the clb
    std::map<AtomBlockId, std::vector<Change_Entry>> cover;
    auto& atom_ctx = g_vpr_ctx.atom();
    auto& device_ctx = g_vpr_ctx.device();
    auto& place_ctx = g_vpr_ctx.mutable_placement();

    //get atoms assigned to current clb
    const std::vector<AtomBlockId> functions = atom_ctx.lookup.clb_atom(blk_id);
    //check if given block/destination location is a clb (contains .names/LUT or .latch/FF), otherwise it is always compatible
    if((!pb_type_contains_blif_model(atom_ctx.lookup.atom_pb_graph_node(*functions.begin())->pb_type, ".names")) &&
       (!pb_type_contains_blif_model(atom_ctx.lookup.atom_pb_graph_node(*functions.begin())->pb_type, ".latch")))
        return true;

    //search map of already found pairs for current mapping
    auto clb = place_ctx.compatibility_mappings.find(blk_id);
    if (clb != place_ctx.compatibility_mappings.end()) {
        auto location = clb->second.begin();
        while (location != clb->second.end()) {
            //if map contains the clb with the same coordinates it has saved the compatibility
            if(location->first.x == loc.x && location->first.y == loc.y && location->first.sub_tile == loc.sub_tile) {
                //if map is empty, it is incompatible
                //otherwise, assign map
                (*map) = location->second;
                return !location->second.empty();
            }
            ++location;
        }
    }

    //get index of block belonging to destination of placement
    unsigned long position = loc.x * device_ctx.grid.width() + loc.y;
    //search for the information about the size and number of luts in the clb
    int num_luts_per_clb = device_ctx.num_luts_per_clb;
    int num_inputs_lut = device_ctx.lut_size;

    //check compatibility for each function in clb
    int num_inputs_fct;
    for (auto fct : functions) {
        //if current truth table has only one entry it is a latch that can always be mapped and needs no consideration
        if(atom_ctx.nlist.block_truth_table(fct).size() == 1) {
            if(atom_ctx.nlist.block_truth_table(fct).begin()->size() == 1) {
                continue;
            }
        }
        //if current function is always 1 or 0 the truthtable could be empty depending if it is on- or off-set encoded
        //this case must be handled specially
        if(atom_ctx.nlist.block_truth_table(fct).empty()) {
            num_inputs_fct = num_inputs_lut;
        }
        else {
            num_inputs_fct = (int) atom_ctx.nlist.block_truth_table(fct).begin()->size() - 1;
        }
        cover.insert(cover.end(), std::pair<AtomBlockId, std::vector<Change_Entry>>(fct, std::vector<Change_Entry>()));
        for (int lut = 0; lut < num_luts_per_clb; ++lut) {
            std::vector<int> perm = std::vector<int>(num_inputs_lut);
            char* error_line = &(lut_errors[position][lut * num_luts_per_clb]);
            //check if current function is compatible to a LUT at the destination
            //subtract 1 of number of function inputs because output value is also saved in truth tables, and it is always single output
            int assigned_lut = check_compatibility_lut(error_line, atom_ctx.nlist.block_truth_table(fct),
                                                       &perm, num_inputs_lut, num_inputs_fct);
            if (assigned_lut != -1) {
                //add necessary permutation to mapping from fct to lut
                cover.find(fct)->second.insert(cover.find(fct)->second.end(), Change_Entry(&perm, lut));
            }
        }
        //if function is incompatible, so is clb
        if(cover.find(fct)->second.empty()) {
            (*map).clear();
            return false;
        }
    }
    //check if a cover from luts and fcts exist and get the necessary permutations
    bool coverable = clb_coverable(map, &cover);
    //empty map symbolizes incompatibility
    if(!coverable)
        map->clear();
    //add the compatibility of this combination to the global map:
    //initialize location vector, if there is none
    if (clb == place_ctx.compatibility_mappings.end()) {
        std::vector<std::pair<t_pl_loc, std::map<AtomBlockId, Change_Entry>>> vec;
        vec.insert(vec.end(), std::pair<t_pl_loc, std::map<AtomBlockId, Change_Entry>>(loc, *map));
        place_ctx.compatibility_mappings.insert(std::pair<ClusterBlockId, std::vector<std::pair<t_pl_loc, std::map<AtomBlockId, Change_Entry>>>>(blk_id, vec));
    }
    //otherwise insert the new mapping-pair
    else
        clb->second.insert(clb->second.end(), std::pair<t_pl_loc, std::map<AtomBlockId, Change_Entry>>(loc, *map));
    return coverable;
}

//Modified: added compatibility check for single function and LUT.
int check_compatibility_lut(const char* error_line, const AtomNetlist::TruthTable table, std::vector<int>* perm, int num_inputs_lut, int num_inputs_fct) {
    //if function requires more inputs than LUT has, its incompatible
    if(num_inputs_fct > num_inputs_lut)
        return -1;
    //init permutation without changes
    *perm = std::vector<int>();
    for (int in = 0; in < num_inputs_lut; ++in) {
        perm->insert(perm->end(), in);
    }
    //transform the truth table into a processable minterm form
    AtomNetlist::TruthTable exp_table = expand_truth_table(table, num_inputs_fct);
    std::vector<vtr::LogicValue> lut_mask = truth_table_to_lut_mask(exp_table, num_inputs_fct);
    //get the number of cells in the lut
    int num_lut_cells = (int) pow(2, num_inputs_lut);
    int num_fct_cells = (int) pow(2, num_inputs_fct);
    //first check: direct mapability
    if(check_compatibility_lut_direct(error_line, lut_mask, num_fct_cells, 0)) {
        return 0;
    }
    //if function needs fewer inputs than lut provides, there are more than one position where the lut can hold it
    if (num_inputs_fct < num_inputs_lut) {
        int num_tries = num_lut_cells / (int) lut_mask.size();
        for (int cur_try = 1; cur_try < num_tries; ++cur_try) {
            if(check_compatibility_lut_direct(error_line, lut_mask, num_fct_cells, cur_try))
                return cur_try;
            else {
                if(g_vpr_ctx.placement().permutation_depth > 0)
                    if (try_find_permutation(error_line, exp_table, *perm, num_inputs_fct, num_fct_cells, num_inputs_lut, g_vpr_ctx.placement().permutation_depth)) {
                        return cur_try;
                }
            }
        }
        return -1;
    }
    else {
        if(g_vpr_ctx.placement().permutation_depth > 0)
            if (try_find_permutation(error_line, exp_table, *perm, num_inputs_fct, num_fct_cells, num_inputs_lut, g_vpr_ctx.placement().permutation_depth)) {
                return 0;
        }
        return -1;
    }
}

//Modified: added check for one lut and one function cell by cell.
bool check_compatibility_lut_direct(const char* error_line, std::vector<vtr::LogicValue> lut_mask, int num_fct_cells, int lut_offset) {
    //test for every single cell if the function is mappable
    for (int cell = 0; cell < num_fct_cells; ++cell) {
        char cur_cell = error_line[cell + lut_offset];
        //differ between fault type of lut-cell
        switch (cur_cell) {
            //Fault free cell: continue
            case '0':
                continue;
            //Stuck-At-1 cell: check if matches lut mask
            case '1': {
                if(lut_mask[cell] == vtr::LogicValue::FALSE)
                    return false;
                continue;
            }
            //Stuck-At-0 cell: check if matches lut mask
            case '2': {
                if (lut_mask[cell] == vtr::LogicValue::TRUE)
                    return false;
                continue;
            }
            //Stuck-At-Undefined cell: not usable, break
            default:
                return false;
        }
    }
    return true;
}

//Modified: added check if a lut becomes mappable by permutation of its inputs
//maximum of 6-input LUTs
bool try_find_permutation(const char* error_line, const AtomNetlist::TruthTable& table, std::vector<int>& perm, int num_inputs_fct, int num_fct_cells, int num_inputs_lut, int permutation_depth){
    auto& place_ctx = g_vpr_ctx.placement();
    auto iter = place_ctx.permutations.begin();
    //iterate over all possible permutations
    for (int cur_perm_index = 0; cur_perm_index < (int) place_ctx.permutations.size(); ++cur_perm_index) {
        int p = std::stoi(iter->first);
        int from = (p/10);
        int to = (p%10);
        //test, if permutation can be applied
        //that is the case, if the inputs the permutation uses exist in the lut
        if (!(from < num_inputs_fct && to < num_inputs_fct))
            continue;
        //apply permutation
        int tmp = perm[from];
        perm[from] = perm[to];
        perm[to] = tmp;
        AtomNetlist::TruthTable permuted_table = permute_truth_table(table, num_inputs_fct, perm);
        std::vector<vtr::LogicValue> permuted_lut_mask = truth_table_to_lut_mask(permuted_table, num_inputs_fct);
        //check if permutation brings success
        if(check_compatibility_lut_direct(error_line, permuted_lut_mask, num_fct_cells, 0))
            return true;
        //if there should be deeper permutations considered, try to find permutation of current state recursively
        if(permutation_depth > 1) {
            if(try_find_permutation(error_line, table, perm, num_inputs_fct, num_fct_cells, num_inputs_lut, permutation_depth - 1))
                return true;
        }
        //reset permutation
        perm.clear();
        for (int in = 0; in < num_inputs_lut; ++in) {
            perm.insert(perm.end(), in);
        }
        ++iter;
    }
    return false;
}

//Modified: added greedy-approach-based check if the clb can cover all the functions of the cluster at once.
bool clb_coverable(std::map<AtomBlockId, Change_Entry>* map, std::map<AtomBlockId, std::vector<Change_Entry>>* cover) {
    if (cover->empty())
        return false;
    AtomBlockId min_fct;
    int num_fcts = (int) cover->size();
    //one iteration per function is necessary
    for (int i = 0; i < num_fcts; ++i) {
        auto cur_fct = cover->begin();
        min_fct = cur_fct->first;
        //search for function that has currently the worst compatibility
        while (cur_fct != cover->end()) {
            if (cover->find(min_fct)->second.size() < cur_fct->second.size()) {
               min_fct = cur_fct->first;
            }
            ++cur_fct;
        }
        //if map with compatible luts is empty, function cannot be mapped and clb is not compatible.
        if(cover->find(min_fct)->second.empty())
            return false;
        else {
            //assign first matching lut to current function and save necessary permutations
            (*map).insert(std::pair<AtomBlockId, Change_Entry>(min_fct, cover->find(min_fct)->second[0]));
            if ((int) map->size() == num_fcts)
                break;
            //get index of lut that is mapped
            int assigned_lut = cover->find(min_fct)->second[0].lut;
            //delete mapped function from cover
            cover->erase(cover->find(min_fct));
            cur_fct = cover->begin();
            //iterate over rest functions and delete all entries that would map to the already assigned LUT.
            while (cur_fct != cover->end()) {
                auto cur_lut = cur_fct->second.begin();
                while (cur_lut != cur_fct->second.end()) {
                    //delete entry in other function with same lut
                    if (cur_lut->lut == assigned_lut) {
                        cur_fct->second.erase(cur_lut);
                        //can leave this map, cover contains for every function each lut only once
                        break;
                    }
                    ++cur_lut;
                }
                ++cur_fct;
            }
        }
    }
    return true;
}