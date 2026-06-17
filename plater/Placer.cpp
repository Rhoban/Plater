#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>
#include <random>
#include <chrono>
#include "Placer.h"
#include "log.h"

namespace Plater
{
    Placer::Placer(Request *request_)
        : solution(NULL),
        myThread(NULL),
        rotateDirection(0),
        scoreMode(PLACER_SCORE_GRAVITY),
        maxPlates(0),
        request(request_)
    {
        for (auto part : request->quantities) {
            for (int i=0; i<part.second; i++) {
                PlacedPart *placedPart = new PlacedPart;
                placedPart->setPart(request->parts[part.first]);
                parts.push_back(placedPart);
            }
        }

        setGravityMode(PLACER_GRAVITY_YX);
    }

    Placer::~Placer()
    {
        if (myThread != NULL) {
            myThread->join();
            delete myThread;
        }
        // After place() the queue is empty (parts are owned by plates). Any
        // parts left here belong to a placer that never ran (e.g. one created
        // only to reuse placePart), so free them.
        for (auto part : parts) {
            delete part;
        }
    }

    void Placer::sortParts(int sortType)
    {
        switch (sortType) {
            case PLACER_SORT_SURFACE_INC:
                sort(parts.begin(), parts.end(), [](const PlacedPart *a, const PlacedPart *b) {
                        return a->getSurface() > b->getSurface();
                        });
                break;
            case PLACER_SORT_SURFACE_DEC:
                sort(parts.begin(), parts.end(), [](const PlacedPart *a, const PlacedPart *b) {
                        return a->getSurface() < b->getSurface();
                        });
                break;
            default:
                unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
                shuffle(parts.begin(), parts.end(), std::default_random_engine(seed));
                break;
        }
    }

    PlacedPart *Placer::getNextPart()
    {
        PlacedPart *part = parts.back();
        parts.pop_back();

        return part;
    }
            
    void Placer::setRotateDirection(int direction)
    {
        rotateDirection = direction;
    }
            
    void Placer::setRotateOffset(int offset)
    {
        rotateOffset = offset;
    }

    void Placer::setScoreMode(int scoreMode_)
    {
        scoreMode = scoreMode_;
    }

    void Placer::setMaxPlates(int maxPlates_)
    {
        maxPlates = maxPlates_;
    }
            
    void Placer::setGravityMode(int gravityMode)
    {
        switch (gravityMode) {
            case PLACER_GRAVITY_YX:
                xCoef = 1;
                yCoef = 10;
                break;
            case PLACER_GRAVITY_XY:
                xCoef = 10;
                yCoef = 1;
                break;
            case PLACER_GRAVITY_EQ:
                xCoef = 1;
                yCoef = 1;
                break;
        }
    }
    
    bool Placer::placePartSkyline(Plate *plate, PlacedPart *part)
    {
        std::string cacheName = part->getName();
        if (cache[plate][cacheName]) {
            return false;
        }

        const float precision = plate->precision;
        const int bw = plate->bmp->width;
        const int bh = plate->bmp->height;
        int dpix = (int)(request->delta / precision);
        if (dpix < 1) {
            dpix = 1;
        }
        const int rs = ceil(M_PI*2/request->deltaR);

        bool found = false;
        float betterScore = 0;
        int betterContact = -1;
        int betterX = 0, betterY = 0, betterR = 0;
        std::vector<int> bottom, top;
        // Outline cells (occupied pixel + outward direction), built per rotation
        // only when contact scoring is active.
        std::vector<int> bpx, bpy, bdx, bdy;
        static const int DIRS[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};

        for (int r=(rotateDirection ? rs-1 : 0); rotateDirection ? r>=0 : r<rs; rotateDirection ? r-- : r++) {
            int vr = (r+rotateOffset)%rs;
            part->setRotation(vr);
            Bitmap *pb = part->getBmp();
            if (pb == NULL) {
                continue;
            }
            const int pw = pb->width;
            const int ph = pb->height;
            if (pw > bw || ph > bh) {
                continue;
            }

            // Per-column bottom/top of the part's footprint for this rotation.
            bottom.assign(pw, -1);
            top.assign(pw, -1);
            for (int px=0; px<pw; px++) {
                for (int py=0; py<ph; py++) {
                    if (pb->getPoint(px, py)) {
                        if (bottom[px] < 0) {
                            bottom[px] = py;
                        }
                        top[px] = py;
                    }
                }
            }

            // For contact scoring, collect the part's outward-facing outline.
            if (scoreMode == PLACER_SCORE_CONTACT) {
                bpx.clear(); bpy.clear(); bdx.clear(); bdy.clear();
                for (int px=0; px<pw; px++) {
                    for (int py=0; py<ph; py++) {
                        if (!pb->getPoint(px, py)) {
                            continue;
                        }
                        for (int d=0; d<4; d++) {
                            if (!pb->getPoint(px+DIRS[d][0], py+DIRS[d][1])) {
                                bpx.push_back(px); bpy.push_back(py);
                                bdx.push_back(DIRS[d][0]); bdy.push_back(DIRS[d][1]);
                            }
                        }
                    }
                }
            }

            const float gx0 = part->getGX();
            const float gy0 = part->getGY();

            for (int X=0; X + pw <= bw; X += dpix) {
                // "Drop" the part onto the skyline: the resting row is the
                // highest contact among the columns it covers. This is
                // overlap-free by construction (everything above colHeight is
                // empty), so no collision test is needed.
                int restingY = 0;
                for (int px=0; px<pw; px++) {
                    if (bottom[px] < 0) {
                        continue;
                    }
                    int need = plate->colHeight[X+px] - bottom[px];
                    if (need > restingY) {
                        restingY = need;
                    }
                }
                if (restingY + ph > bh) {
                    continue;
                }

                float gx = gx0 + X*precision;
                float gy = gy0 + restingY*precision;
                float score = gy*yCoef + gx*xCoef;

                bool better;
                int contact = 0;
                if (scoreMode == PLACER_SCORE_CONTACT) {
                    // Count outline cells whose outward neighbour is the bed
                    // edge (a wall) or an already-occupied pixel (a part).
                    for (size_t i=0; i<bpx.size(); i++) {
                        int qx = X + bpx[i] + bdx[i];
                        int qy = restingY + bpy[i] + bdy[i];
                        if (qx < 0 || qx >= bw || qy < 0 || qy >= bh) {
                            contact++;
                        } else if (plate->bmp->getPoint(qx, qy)) {
                            contact++;
                        }
                    }
                    // Prefer more contact, breaking ties with gravity.
                    better = !found || contact > betterContact ||
                             (contact == betterContact && score < betterScore);
                } else {
                    better = !found || score < betterScore;
                }

                if (better) {
                    found = true;
                    betterScore = score;
                    betterContact = contact;
                    betterX = X;
                    betterY = restingY;
                    betterR = vr;
                }
            }
        }

        if (found) {
            part->setRotation(betterR);
            part->setOffset(betterX*precision, betterY*precision);
            plate->place(part);
            return true;
        } else {
            cache[plate][cacheName] = true;
            return false;
        }
    }

    bool Placer::placePartPruned(Plate *plate, PlacedPart *part)
    {
        std::string cacheName = part->getName();
        if (cache[plate][cacheName]) {
            return false;
        }

        int rs = ceil(M_PI*2/request->deltaR);
        bool found = false;
        float betterScore = 0;
        float betterX = 0, betterY = 0;
        int betterR = 0;

        for (int r=(rotateDirection ? rs-1 : 0); rotateDirection ? r>=0 : r<rs; rotateDirection ? r-- : r++) {
            int vr = (r+rotateOffset)%rs;
            part->setRotation(vr);
            if (part->getBmp() == NULL) {
                continue;
            }
            float gx0 = part->getGX();
            float gy0 = part->getGY();

            for (float x=0; x<plate->width; x+=request->delta) {
                float gx = gx0 + x;
                // The lowest possible score for this column is at y=0. Because
                // gx grows with x, once a column's best case can't beat the
                // current best, no later column (this rotation) can either.
                float colMin = gy0*yCoef + gx*xCoef;
                if (found && colMin >= betterScore) {
                    break;
                }
                for (float y=0; y<plate->height; y+=request->delta) {
                    float score = (gy0+y)*yCoef + gx*xCoef;
                    if (found && score >= betterScore) {
                        break;  // higher y only scores worse -> next column
                    }
                    part->setOffset(x, y);
                    if (plate->canPlace(part)) {
                        // First feasible cell in a column is its best (lowest)
                        // score, and it strictly beats the current best (else we
                        // would have broken above). Full collision keeps this
                        // hole-aware, identical to the brute-force result.
                        found = true;
                        betterScore = score;
                        betterX = x;
                        betterY = y;
                        betterR = vr;
                        break;
                    }
                }
            }
        }

        if (found) {
            part->setRotation(betterR);
            part->setOffset(betterX, betterY);
            plate->place(part);
            return true;
        } else {
            cache[plate][cacheName] = true;
            return false;
        }
    }

    bool Placer::placePart(Plate *plate, PlacedPart *part)
    {
        if (request->skyline && plate->mode == PLATE_MODE_RECTANGLE) {
            return placePartSkyline(plate, part);
        }
        if (request->prunedBrute) {
            return placePartPruned(plate, part);
        }

        std::string cacheName = part->getName();

        if (!cache[plate][cacheName]) {
            float betterX=0, betterY=0, betterScore;
            int betterR=0;
            int rs = ceil(M_PI*2/request->deltaR);
            bool found = false;

            for (int r=(rotateDirection ? rs-1 : 0); rotateDirection ? r>=0 : r<rs; rotateDirection ? r-- : r++) {
                int vr = (r+rotateOffset)%rs;
                part->setRotation(vr);
                if (part->getBmp() != NULL) {
                    for (float x=0; x<plate->width; x+=request->delta) {
                        for (float y=0; y<plate->height; y+=request->delta) {
                            float gx = part->getGX()+x;
                            float gy = part->getGY()+y;
                            float score = gy*yCoef+gx*xCoef;

                            if (!found || score < betterScore) {
                                part->setOffset(x, y);
                                if (plate->canPlace(part)) {
                                    found = true;
                                    betterX = x;
                                    betterY = y;
                                    betterScore = score;
                                    betterR = vr;
                                }
                            }
                        }
                    }
                }
            }
            if (found) {
                // _log("- Placing it @%g,%g r=%d\n", betterX, betterY, betterR);
                part->setRotation(betterR);
                part->setOffset(betterX, betterY);
                plate->place(part);
                return true;
            } else {
                cache[plate][cacheName] = true;
                return false;
            }
        } else {
            return false;
        }
    }

    Solution *Placer::place()
    {
        Solution *solution = new Solution(request->plateWidth, request->plateHeight, request->plateDiameter, request->plateMode, request->precision);
        solution->addPlate();

        _log("* Placer\n");
        bool failed = false;
        while (parts.size() && !failed) {
            PlacedPart *part = getNextPart();
            // _log("- Trying to place %s...\n", part->getPart()->getFilename().c_str());
            bool placed = false;

            for (int i=0; i<solution->countPlates() && !placed; i++) {
                Plate *plate = solution->getPlate(i);

                if (placePart(plate, part)) {
                    placed = true;
                } else if (i+1 == solution->countPlates()) {
                    // Last existing plate didn't fit it; add one unless capped.
                    if (maxPlates > 0 && solution->countPlates() >= maxPlates) {
                        break;
                    }
                    // _log("! Creating a new plate\n");
                    solution->addPlate();
                }
            }

            if (!placed) {
                // Only reachable when capped: the part won't fit in the cap.
                delete part;
                failed = true;
            }
        }

        if (failed) {
            for (auto p : parts) {
                delete p;
            }
            parts.clear();
            delete solution;
            this->solution = NULL;
            return NULL;
        }

        _log("- Solution with %d plates\n", solution->countPlates());

        this->solution = solution;
        return solution;
    }
            
    void Placer::placeThreaded()
    {
        myThread = new std::thread([this](){
            this->place();
        });
    }
}
