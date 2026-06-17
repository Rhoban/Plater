#define _USE_MATH_DEFINES
#include <math.h>
#include <iostream>
#include "Part.h"

using namespace std;

namespace Plater
{
    Part::Part()
        : bmp(NULL), allBmp(NULL)
    {
    }

    Part::~Part()
    {
        // allBmp owns the bitmaps; bmp only holds (aliased) views into it.
        if (allBmp != NULL) {
            for (int i=0; i<bmps; i++) {
                if (allBmp[i] != NULL) {
                    delete allBmp[i];
                }
            }
            delete[] allBmp;
        }
        if (bmp != NULL) {
            delete[] bmp;
        }
    }

    int Part::load(std::string filename_, float precision_, float deltaR_, float spacing,
            string orientation, float plateWidth, float plateHeight)
    {
        precision = precision_;
        deltaR = deltaR_;
        bmps = ceil((M_PI*2)/deltaR);
        filename = filename_;

        // Size-independent geometry work (disk read, pixelize, rotate, trim):
        // done once. allBmp keeps every rotation; the per-plate-size filtering
        // lives in applyPlateSize so a fit search can re-filter cheaply.
        model = loadModelFromFile(filename.c_str());
        model = model.putFaceOnPlate(orientation);
        allBmp = new Bitmap*[bmps];
        bmp = new Bitmap*[bmps];
        allBmp[0] = model.pixelize(precision, spacing);

        Point3 minP = model.min();
        Point3 maxP = model.max();
        width = maxP.x-minP.x + 2*spacing;
        height = maxP.y-minP.y + 2*spacing;

        for (int k=1; k<bmps; k++) {
            Bitmap *rotated = Bitmap::rotate(allBmp[0], k*deltaR);
            allBmp[k] = Bitmap::trim(rotated);
            delete rotated;
        }

        return applyPlateSize(plateWidth, plateHeight);
    }

    int Part::applyPlateSize(float plateWidth, float plateHeight)
    {
        surface = 0;
        int correct = 0;

        for (int k=0; k<bmps; k++) {
            // Will this rotation fit on the plate ?
            if (allBmp[k]->width*precision < plateWidth && allBmp[k]->height*precision < plateHeight) {
                bmp[k] = allBmp[k];
                surface += allBmp[k]->width * allBmp[k]->height;
                correct++;
            } else {
                bmp[k] = NULL;
            }
        }

        if (correct > 0) {
            surface /= (float)correct;
        }
        return correct;
    }
            
    float Part::getSurface() const
    {
        return surface;
    }

    std::string Part::getFilename()
    {
        return filename;
    }
            
    Bitmap *Part::getBmp(int index) const
    {
        return bmp[index];
    }
            
    float Part::getDensity(int index) const
    {
        Bitmap *bmp = getBmp(index);
        return bmp->pixels/(bmp->width*bmp->height);
    }
}
