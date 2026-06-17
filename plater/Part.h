#ifndef _PLATER_PART_H
#define _PLATER_PART_H

#include <iostream>
#include "stl/StlFactory.h"


namespace Plater
{
    class Part
    {
        public:
            Part();
            virtual ~Part();

            int load(std::string filename, float precision, float deltaR, float spacing, string orientation,
                float plateWidth, float plateHeight);
            // Recompute which rotations fit on a plate of the given size and the
            // average surface, without reloading/re-pixelizing the geometry.
            // Returns the number of usable rotations (0 means it doesn't fit).
            int applyPlateSize(float plateWidth, float plateHeight);
            std::string getFilename();

            Model model;

            Bitmap *getBmp(int index) const;
            float getSurface() const;
            float getDensity(int index) const;

            float precision;
            float deltaR;

            float width;
            float height;
            float surface;

            int bmps;
            // Per-rotation bitmap "view": points into allBmp for rotations that
            // fit the current plate size, or NULL for those that don't.
            Bitmap **bmp;
            // Owns every rotation bitmap (size-independent); never filtered.
            Bitmap **allBmp;

        protected:
            std::string filename;
    };
}

#endif
