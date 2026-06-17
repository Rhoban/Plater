#include <iostream>
#include <fcntl.h>
#if defined(_WIN32) || defined(_WIN64)
#include "wingetopt.h"
#else
#include <getopt.h>
#endif
#define _USE_MATH_DEFINES
#include <math.h>
#include "stl/StlFactory.h"
#include "Request.h"
#include "util.h"
#include "log.h"

using namespace std;
using namespace Plater;

void help()
{
    cerr << "Plater v1.0 (https://github.com/RobotsWar/Plater)" << endl;
    cerr << "Usage: plater [options] plater.conf" << endl;
    cerr << "(Use - to read from stdin)" << endl;
    cerr << endl;
    cerr << "-h: Display this help" << endl;
    cerr << "-v: Verbose mode" << endl;
    cerr << "The size of the bed plate (topview, 2D):" << endl;
    cerr << "  -W width: Setting the plate width (default: 150mm)" << endl;
    cerr << "  -H height: Setting the plate height (default: 150mm)" << endl;
    cerr << "-D diameter: Set the plate diameter, in mm. If set, this will put the plate in circular mode" << endl;
    cerr << "-j precision: Sets the precision (in mm, default: 0.5)" << endl;
    cerr << "-s spacing: Change the spacing between parts (in mm, default: 1.5)" << endl;
    cerr << "-d delta: Sets the interval of place grid (in mm, default: 1.5)" << endl;
    cerr << "-r rotation: Sets the interval of rotation (in °, default: 90)" << endl;
    cerr << "-S: Trying multiple sort possibilities" << endl;
    cerr << "-R random: Sets the number of random (shuffled parts) iterations (only with -S)" << endl;
    cerr << "-o pattern: output file pattern (default: plate_%03d)" << endl;
    cerr << "-p: will output ppm of the plates" << endl;
    cerr << "-m: output a single 3MF file containing all the plates" << endl;
    cerr << "-A algorithm: placement algorithm (default: brute). One of:" << endl;
    cerr << "     brute    - original full brute force (hole-aware)" << endl;
    cerr << "     pruned   - hole-aware pruned brute force (same packing as brute, faster)" << endl;
    cerr << "     skyline  - skyline bottom-left drop (fastest; no hole filling; rectangular plates)" << endl;
    cerr << "     contact  - skyline with max-contact scoring (denser; no hole filling)" << endl;
    cerr << "-C: consolidation pass - try to empty the sparsest plate into the others' gaps" << endl;
    cerr << "Fit search (grow the plate from an ideal size up to the bed size -W/-H):" << endl;
    cerr << "  -i ideal: ideal (smallest) plate size in mm; enables the fit search" << endl;
    cerr << "  -g step: growth step in mm while searching (default: 10)" << endl;
    cerr << "  -N plates: number of plates to target first, grows if needed (default: 1)" << endl;
    cerr << "-t threads: sets the number of threads (default 1)" << endl;
    cerr << "-c: enables the output of plates.csv containing plates infos" << endl;
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    int index;
    Request request;

    bool fitMode = false;
    double fitIdeal = 0;
    double fitStep = 10;
    int fitTarget = 1;

    while ((index = getopt(argc, argv, "hvs:d:r:pmA:Cj:d:o:W:H:R:D:t:Sci:g:N:")) != -1) {
        switch (index) {
            case 'h':
                help();
                break;
            case 'v':
                increaseVerboseLevel();
                break;
            case 's':
                request.spacing = atof(optarg)*1000;
                break;
            case 'd':
                request.delta = atof(optarg)*1000;
                break;
            case 'r':
                request.deltaR = DEG2RAD(atof(optarg));
                break;
            case 'p':
                request.mode = REQUEST_PPM;
                break;
            case 'm':
                request.mode = REQUEST_3MF;
                break;
            case 'A': {
                string algo = string(optarg);
                if (algo == "brute") {
                    // defaults: no flags
                } else if (algo == "pruned") {
                    request.prunedBrute = true;
                } else if (algo == "skyline") {
                    request.skyline = true;
                } else if (algo == "contact") {
                    request.skyline = true;
                    request.contact = true;
                } else {
                    cerr << "Unknown algorithm '" << algo
                         << "' (expected: brute, pruned, skyline, contact)" << endl;
                    help();
                }
                break;
            }
            case 'C':
                request.consolidate = true;
                break;
            case 'j':
                request.precision = atof(optarg)*1000;
                break;
            case 'o':
                request.pattern = string(optarg);
                break;
            case 'W':
                request.plateWidth = atof(optarg)*1000;
                break;
            case 'H':
                request.plateHeight = atof(optarg)*1000;
                break;
            case 'S':
                request.sortMode = REQUEST_MULTIPLE_SORTS;
                break;
            case 'R':
                request.randomIterations = atoi(optarg);
                break;
            case 'D':
                request.plateMode = PLATE_MODE_CIRCLE;
                request.plateDiameter = atof(optarg)*1000;
                break;
            case 't':
                request.nbThreads = atoi(optarg);
                break;
            case 'c':
                request.platesInfo = true;
                break;
            case 'i':
                fitIdeal = atof(optarg);
                fitMode = true;
                break;
            case 'g':
                fitStep = atof(optarg);
                break;
            case 'N':
                fitTarget = atoi(optarg);
                break;
        }
    }

    if (optind != argc) {
        string filename = string(argv[optind]);
        if (filename == "-") {
            request.readFromStdin();
        } else {
            request.readFromFile(filename);
        }
    } else {
        help();
    }

    if (fitMode) {
        request.processFit(fitIdeal, fitStep, fitTarget);
    } else {
        request.process();
    }

    return EXIT_SUCCESS;
}
