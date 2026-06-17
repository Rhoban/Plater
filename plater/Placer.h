#ifndef _PLATER_PLACER_H
#define _PLATER_PLACER_H

#include <thread>
#include <map>
#include "Request.h"
#include "Plate.h"
#include "PlacedPart.h"
#include "Solution.h"

#define PLACER_SORT_SURFACE_DEC 0
#define PLACER_SORT_SURFACE_INC 1
#define PLACER_SORT_SHUFFLE     2

#define PLACER_GRAVITY_YX       0
#define PLACER_GRAVITY_XY       1
#define PLACER_GRAVITY_EQ       2

// Candidate scoring for the skyline placer
#define PLACER_SCORE_GRAVITY    0
#define PLACER_SCORE_CONTACT    1

namespace Plater
{
    class Placer
    {
        public:
            Placer(Request *request);
            virtual ~Placer();

            void sortParts(int sortType);
            void setGravityMode(int gravityMode);
            void setRotateDirection(int direction);
            void setRotateOffset(int offset);
            void setScoreMode(int scoreMode);
            // Cap the number of plates; place() returns NULL if the parts don't
            // fit within the cap. 0 means no cap (default).
            void setMaxPlates(int maxPlates);

            PlacedPart *getNextPart();
            Solution *place();
            void placeThreaded();
            Solution *solution;

            // Search for a spot for `part` on `plate` and place it if one is
            // found (respects the configured algorithm). Used directly by the
            // consolidation pass.
            bool placePart(Plate *plate, PlacedPart *part);

        protected:
            std::thread *myThread;
            int rotateOffset;
            int rotateDirection;
            int scoreMode;
            int maxPlates;
            std::map<Plate *, std::map<std::string, bool> > cache;
            float xCoef, yCoef;
            vector<PlacedPart *> parts;
            Request *request;

            bool placePartSkyline(Plate *plate, PlacedPart *part);
            bool placePartPruned(Plate *plate, PlacedPart *part);
    };
}

#endif
