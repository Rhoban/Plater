#ifndef _PLATER_THREEMF_H
#define _PLATER_THREEMF_H

#include <string>
#include "Solution.h"

namespace Plater
{
    /**
     * Write every plate of a solution into a single 3MF file using the
     * OrcaSlicer / BambuStudio multi-plate project layout.
     *
     * Each placed part becomes its own 3MF object named after the original
     * part file. Plates are described in Metadata/model_settings.config and
     * laid out on OrcaSlicer's plate grid (stride = 1.2 * plate size) so each
     * plate opens as a separate plate in the slicer.
     *
     * plateWidth / plateDepth are the bed dimensions in millimeters; they must
     * match the slicer's bed so the plate grid lines up.
     *
     * Returns true on success.
     */
    bool saveSolutionTo3MF(const std::string &filename, Solution *solution,
            double plateWidth, double plateDepth);
}

#endif
