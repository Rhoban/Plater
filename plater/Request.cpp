#define _USE_MATH_DEFINES
#include <set>
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
        platesInfo(false)
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
            stream = &ifile;
            readParts();
        }
    }

    void Request::readFromStdin()
    {
        _log("* Reading request from stdin\n");
        stream = &cin;
        readParts();
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
        switch (mode) {
            case REQUEST_PPM:
                pattern += ".ppm";
                break;
            case REQUEST_STL:
                pattern += ".stl";
                break;
        }

        // Writing plates info
        if (platesInfo) {
            _log("- Exporting plates.csv...\n");
            writePlatesInfo(solution);
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

    void Request::process()
    {
        if (solution != NULL) {
            Solution *toDelete = solution;
            solution = NULL;
            delete toDelete;
        }

        if (!cancel) {
            if (hasError) {
                cerr << "! Can't process: " << error << endl;
            } else {
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

                if (!cancel) {
                    _log("* Solution\n");
                    _log("- Plates: %d\n", solution->countPlates());
                    _log("- Score: %g\n", solution->score());
                    writeFiles(solution);
                    plates = solution->countPlates();
                }
            }
        }
    }
}
