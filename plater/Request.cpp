#define _USE_MATH_DEFINES
#include <set>
#include <algorithm>
#include <math.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include "util.h"
#include "Request.h"
#include "Placer.h"
#include "Plate.h"
#include "Solution.h"
#include "ThreeMF.h"
#include "log.h"
#include "sleep.h"

using namespace std;

#if defined(_WIN32) || defined(_WIN64)
#define snprintf _snprintf
#endif

namespace Plater
{
    Request::Request()
        : 
        plateMode(PLATE_MODE_RECTANGLE),
        plateWidth(150000),
        plateHeight(150000),
        randomIterations(3),
        mode(REQUEST_STL),
        sortMode(REQUEST_SINGLE_SORT),
        precision(500),
        delta(1000),
        deltaR(M_PI/2),
        spacing(1500),
        pattern("plate_%03d"),
        cancel(false),
        solution(NULL),
        nbThreads(1),
        platesInfo(false),
        skyline(false),
        contact(false),
        prunedBrute(false),
        consolidate(false)
    {
    }

    Request::~Request()
    {
        for (auto part : parts) {
            delete part.second;
        }

        if (solution != NULL) {
            delete solution;
        }
    }
            
    std::string Request::readLine()
    {
        char buffer[4096];
        stream->getline(buffer, 4096);

        return string(buffer);
    }

    void Request::setPlateSize(float w, float h)
    {
        plateWidth = w*1000;
        plateHeight = h*1000;
    }

    void Request::addPart(std::string filename, int quantity, string orientation)
    {
        if (!cancel && !hasError) {
            if (filename != "" && quantity != 0) {
                _log("- Loading %s (quantity %d, orientation %s)...\n", filename.c_str(), quantity, orientation.c_str());
                parts[filename] = new Part;
                int loaded = 
                    parts[filename]->load(filename, precision, deltaR, spacing, orientation, plateWidth, plateHeight);
                quantities[filename] = quantity;

                if (loaded == 0) {
                    ostringstream oss;
                    oss << "Part " << filename << " is too big for the plate ";
                    oss << " (bed too small? try more angles?)";
                    error = oss.str();
                    hasError = true;
                }
            }
        }
    }
            
    void Request::readPartsFromString(std::string parts)
    {
        partsText = parts;
        istringstream s(parts);

        stream = &s;
        readParts();
    }

    /**
     * Read chunks from line, could be:
     *
     * file.stl quantity
     *      or
     * file.stl quantity orientation
     */
    vector<string> Request::getChunks(string line)
    {
        vector<string> parts = split(line, ' ');
        vector<string> chunks;
        int n = parts.size();
        string filename;

        if (n < 1) {
            return parts;
        }

        // XXX: This is not really clean, something smarter could 
        // be done here

        int quantity;
        for (quantity=n-1; quantity>0; quantity--) {
            if (isNumeric(parts[quantity])) {
                break;
            }
        }
        for (int i=0; i<quantity; i++) {
            if (filename != "") {
                filename += " ";
            }
            filename += parts[i];
        }
        chunks.push_back(filename);
        for (int i=quantity; i<n; i++) {
            chunks.push_back(parts[i]);
        }

        return chunks;
    }

    void Request::readParts()
    {
        parts.clear();
        quantities.clear();

        hasError = false;
        while (!stream->eof()) {
            string line = readLine();
            if (line[0] != '#') {
                line = trim(line);
                vector<string> chunks = getChunks(line);
                if (chunks.size() > 0) {
                    string filename = chunks[0];
                    int quantity = 1;
                    string orientation = "bottom";

                    if (chunks.size() >= 2) quantity = atof(chunks[1].c_str());

                    if (chunks.size() >= 3) {
                        orientation = chunks[2];
                    }

                    try {
                        addPart(filename, quantity, orientation);
                    } catch (string error_) {
                        hasError = true;
                        error = error_;
                        return;
                    }
                }
            }
        }
    }
            
    void Request::readFromFile(std::string filename)
    {
        if (!chdirFile(filename)) {
            cerr << "! Can't go to the directory of " << filename << endl;
        }
        ifstream ifile(getBasename(filename));

        if (!ifile) {
            cerr << "! Can't open configuration file " << filename << endl;
        } else {
            cerr << "* Reading from " << filename << endl;
            // Read the whole file so the parts can be reloaded later (e.g.
            // processFit reloads at different plate sizes).
            stringstream buffer;
            buffer << ifile.rdbuf();
            readPartsFromString(buffer.str());
        }
    }

    void Request::readFromStdin()
    {
        _log("* Reading request from stdin\n");
        stringstream buffer;
        buffer << cin.rdbuf();
        readPartsFromString(buffer.str());
    }
    
    void Request::writeSTL(Plate *plate, const char *filename)
    {
        Model model = plate->createModel();

        try {
            saveModelToFileBinary(filename, &model);
        } catch (string error_) {
            hasError = true;
            error = error_;
        }
    }

    void Request::write3MF(Solution *solution, const char *filename)
    {
        // Plate (bed) dimensions in mm; for a circular bed use the diameter
        // as the bounding square so the slicer grid still lines up.
        double w = (plateMode == PLATE_MODE_CIRCLE ? plateDiameter : plateWidth) / 1000.0;
        double d = (plateMode == PLATE_MODE_CIRCLE ? plateDiameter : plateHeight) / 1000.0;

        if (!saveSolutionTo3MF(filename, solution, w, d)) {
            ostringstream oss;
            oss << "Can't write to " << filename;
            error = oss.str();
            hasError = true;
            logError("Error: can't write to %s\n", filename);
        }
    }

    void Request::writePpm(Plate *plate, const char *filename)
    {
        ofstream ofile(filename);
        if (ofile) {
            ofile << plate->bmp->toPpm();
            ofile.close();
        } else {
            ostringstream oss;
            oss << "Can't write to " << filename;
            error = oss.str();
            hasError = true;
            logError("Error: can't write to %s\n", filename);
        }
    }

    void Request::writePlatesInfo(Solution *solution)
    {
        std::ofstream ofs("plates.csv");

        ofs << "plate,part,posX,posY,rotation" << std::endl;

        for (int i=0; i<solution->countPlates(); i++) {
            Plate *plate = solution->getPlate(i);

            for (auto part : plate->parts) {
                ofs << i+1 << ",";
                ofs << part->getName() << ",";
                ofs << part->getCenterX()/1000.0 << ",";
                ofs << part->getCenterY()/1000.0 << ",";
                ofs << (part->getRotation()*part->getPart()->deltaR*180.0/M_PI) << "";
                ofs << std::endl;

            }
        }

        ofs.close();
    }

    void Request::writeFiles(Solution *solution)
    {
        generatedFiles.clear();

        _log("* Exporting\n");

        // Writing plates info
        if (platesInfo) {
            _log("- Exporting plates.csv...\n");
            writePlatesInfo(solution);
        }

        // 3MF packs every plate into a single file rather than one file
        // per plate, so it is handled on its own.
        if (mode == REQUEST_3MF) {
            string out = pattern;
            // Drop any printf-style placeholder (and trailing separators)
            // since there is only one output file.
            size_t pos = out.find('%');
            if (pos != string::npos) {
                out = out.substr(0, pos);
            }
            while (!out.empty() &&
                   (out.back() == '_' || out.back() == '-' ||
                    out.back() == '.' || out.back() == ' ')) {
                out.pop_back();
            }
            if (out.empty()) {
                out = "plates";
            }
            out += ".3mf";

            _log("- Exporting %s...\n", out.c_str());
            generatedFiles.push_back(out);
            write3MF(solution, out.c_str());
            return;
        }

        switch (mode) {
            case REQUEST_PPM:
                pattern += ".ppm";
                break;
            case REQUEST_STL:
                pattern += ".stl";
                break;
        }

        // Exporting each file
        char *buffer = new char[pattern.size()+64];
        for (int i=0; i<solution->countPlates(); i++) {
            Plate *plate = solution->getPlate(i);
            int plateNumber = i+1;
            snprintf(buffer, pattern.size()+63, pattern.c_str(), plateNumber);
            _log("- Exporting %s...\n", buffer);
            generatedFiles.push_back(string(buffer));

            switch (mode) {
                case REQUEST_STL:
                    writeSTL(plate, buffer);
                    break;
                case REQUEST_PPM:
                    writePpm(plate, buffer);
                    break;
            }
        }
        delete[] buffer;
    }

    void Request::solve()
    {
        if (solution != NULL) {
            Solution *toDelete = solution;
            solution = NULL;
            delete toDelete;
        }

        if (cancel || hasError) {
            return;
        }

        if (plateMode == PLATE_MODE_RECTANGLE) {
            _log("- Plate size: %g x %g microm\n", plateWidth, plateHeight);
        } else {
            _log("- Plate size: %g microm (circle)\n", plateDiameter);
        }

        int lastSort;
        if (sortMode == REQUEST_SINGLE_SORT) {
            lastSort = PLACER_SORT_SURFACE_DEC;
        } else {
            lastSort = PLACER_SORT_SHUFFLE+randomIterations;
        }
        vector<Placer*> placers;
        for (int sortMode=0; sortMode<=lastSort; sortMode++) {
            for (int rotateOffset=0; rotateOffset<2; rotateOffset++) {
                for (int rotateDirection=0; rotateDirection<2; rotateDirection++) {
                    for (int gravity=0; gravity<PLACER_GRAVITY_EQ; gravity++) {
                        Placer *placer = new Placer(this);
                        placer->sortParts(sortMode);
                        placer->setGravityMode(gravity);
                        placer->setRotateDirection(rotateDirection);
                        placer->setRotateOffset(rotateOffset);
                        placers.push_back(placer);
                    }

                    // Add a max-contact scored skyline placer for this config.
                    // The solver keeps the best result, so these only ever help.
                    if (skyline && contact) {
                        Placer *placer = new Placer(this);
                        placer->sortParts(sortMode);
                        placer->setGravityMode(PLACER_GRAVITY_YX);
                        placer->setRotateDirection(rotateDirection);
                        placer->setRotateOffset(rotateOffset);
                        placer->setScoreMode(PLACER_SCORE_CONTACT);
                        placers.push_back(placer);
                    }
                }
            }
        }
        placersCount = placers.size();
        placerCurrent = 0;

        bool stop = false;
        std::set<Placer*> workers;

        while (placers.size() || workers.size()) {
            while (placers.size() && workers.size() < nbThreads) {
                Placer *placer = placers.back();
                placers.pop_back();

                if (!stop && !cancel) {
                    workers.insert(placer);
                    placer->placeThreaded();
                }
            }

            vector<Placer*> toDelete;
            for (auto placer : workers) {
                if (placer->solution != NULL) {
                    Solution *solutionTmp = placer->solution;

                    if (solution == NULL || solutionTmp->score() < solution->score()) {
                        solution = solutionTmp;
                    } else {
                        delete solutionTmp;
                    }

                    if (solution->countPlates() == 1) {
                        stop = true;
                    }

                    placerCurrent++;
                    toDelete.push_back(placer);
                }
            }

            for (auto placer : toDelete) {
                workers.erase(placer);
                delete placer;
            }


            ms_sleep(50);
        }

        if (!cancel && solution != NULL) {
            plates = solution->countPlates();
        }
    }

    void Request::consolidateSolution()
    {
        if (solution == NULL) {
            return;
        }

        // Try to repack every part into one fewer plate, then keep going. The
        // greedy already first-fits each part onto the earliest plate, so the
        // only way to drop a plate is to re-pack with rearrangement; we explore
        // a few orderings (the base run may have used just one) and accept a
        // result only when it strictly reduces the plate count.
        bool improved = true;
        while (improved && solution->countPlates() > 1) {
            improved = false;
            int target = solution->countPlates() - 1;

            std::vector<int> orderings;
            orderings.push_back(PLACER_SORT_SURFACE_DEC);
            orderings.push_back(PLACER_SORT_SURFACE_INC);
            for (int s=0; s<16; s++) {
                orderings.push_back(PLACER_SORT_SHUFFLE);
            }

            for (int ordering : orderings) {
                Placer placer(this);
                placer.setMaxPlates(target);
                placer.sortParts(ordering);
                placer.setGravityMode(PLACER_GRAVITY_YX);
                placer.setRotateOffset(0);
                placer.setRotateDirection(0);

                Solution *candidate = placer.place();
                if (candidate != NULL && candidate->countPlates() <= target) {
                    _log("- Consolidated: %d -> %d plates\n",
                            target + 1, candidate->countPlates());
                    delete solution;
                    solution = candidate;
                    improved = true;
                    break;
                } else if (candidate != NULL) {
                    delete candidate;
                }
            }
        }

        if (!cancel && solution != NULL) {
            plates = solution->countPlates();
        }
    }

    void Request::process()
    {
        if (hasError) {
            cerr << "! Can't process: " << error << endl;
            return;
        }

        solve();

        if (consolidate) {
            consolidateSolution();
        }

        if (!cancel && !hasError && solution != NULL) {
            _log("* Solution\n");
            _log("- Plates: %d\n", solution->countPlates());
            _log("- Score: %g\n", solution->score());
            writeFiles(solution);
        }
    }

    void Request::processFit(double idealMm, double stepMm, int targetPlates)
    {
        if (cancel) {
            return;
        }
        if (targetPlates < 1) {
            targetPlates = 1;
        }

        // The configured plate size is the physical maximum to grow into.
        bool circle = (plateMode == PLATE_MODE_CIRCLE);
        double maxW = circle ? plateDiameter : plateWidth;
        double maxH = circle ? plateDiameter : plateHeight;
        double ideal = idealMm * 1000.0;
        double step = stepMm * 1000.0;
        if (step <= 0) {
            step = 10000.0;
        }

        // Discrete grid of candidate sizes, from the ideal up to the bed size.
        std::vector<double> sizes;
        for (double s = ideal; ; s += step) {
            sizes.push_back(s);
            if (s >= maxW && s >= maxH) {
                break;
            }
        }

        // Apply a candidate size: set the plate dimensions (clamped to the bed)
        // and cheaply re-filter every already-loaded part. Returns false if a
        // part no longer fits at this size.
        auto applySize = [&](double size) -> bool {
            if (circle) {
                plateDiameter = size < maxW ? size : maxW;
            } else {
                plateWidth = size < maxW ? size : maxW;
                plateHeight = size < maxH ? size : maxH;
            }
            double w = circle ? plateDiameter : plateWidth;
            double h = circle ? plateDiameter : plateHeight;
            bool feasible = true;
            for (auto &kv : parts) {
                if (kv.second->applyPlateSize(w, h) == 0) {
                    feasible = false;
                }
            }
            return feasible;
        };

        // Solve at a candidate size; returns the plate count, or -1 if a part
        // cannot fit at that size.
        auto platesAt = [&](double size) -> int {
            if (!applySize(size)) {
                return -1;
            }
            _log("* Trying plate size %g x %g mm\n",
                    (circle ? plateDiameter : plateWidth) / 1000.0,
                    (circle ? plateDiameter : plateHeight) / 1000.0);
            solve();
            int n = (solution != NULL) ? solution->countPlates() : -1;
            if (n > 0) {
                _log("- Fits in %d plate(s)\n", n);
            }
            return n;
        };

        // Load the parts once, at the maximum (bed) size. Geometry is loaded
        // here; subsequent sizes only re-run the cheap fit filter.
        if (circle) {
            plateDiameter = maxW;
        } else {
            plateWidth = maxW;
            plateHeight = maxH;
        }
        hasError = false;
        error = "";
        readPartsFromString(partsText);
        if (hasError) {
            cerr << "! Can't process: " << error << endl;
            return;
        }

        // Fewest plates is achieved at the largest size. We accept the target
        // count, grown to whatever is actually reachable ("start at target,
        // add a plate only if forced").
        int nMax = platesAt(sizes.back());
        if (nMax < 0) {
            hasError = true;
            error = "Parts don't fit even at the maximum plate size";
            cerr << "! " << error << endl;
            return;
        }
        int acceptable = targetPlates > nMax ? targetPlates : nMax;

        // Binary-search the smallest grid size whose plate count is within the
        // acceptable bound (plate count is monotonic non-increasing in size).
        int lo = 0, hi = (int)sizes.size() - 1, chosen = hi;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            int n = platesAt(sizes[mid]);
            if (n > 0 && n <= acceptable) {
                chosen = mid;
                hi = mid - 1;
            } else {
                lo = mid + 1;
            }
        }

        // Ensure the final solution corresponds to the chosen size.
        platesAt(sizes[chosen]);

        if (consolidate) {
            consolidateSolution();
        }

        if (!cancel && !hasError && solution != NULL) {
            _log("* Solution\n");
            _log("- Packed size: %g x %g mm\n",
                    (circle ? plateDiameter : plateWidth) / 1000.0,
                    (circle ? plateDiameter : plateHeight) / 1000.0);
            _log("- Plates: %d\n", solution->countPlates());
            _log("- Score: %g\n", solution->score());

            // Restore the physical bed so the 3MF plate grid and declared bed
            // match the real printer bed; the fit search only governs how
            // tightly the parts pack within each plate.
            if (circle) {
                plateDiameter = maxW;
            } else {
                plateWidth = maxW;
                plateHeight = maxH;
            }
            writeFiles(solution);
        }
    }
}
