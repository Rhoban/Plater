#ifndef _PLATER_REQUEST_H
#define _PLATER_REQUEST_H

#include <map>
#include <sstream>
#include <iostream>
#include <vector>
#include "Part.h"
#include "Solution.h"
#include "PlacedPart.h"

// Output modes
#define REQUEST_STL 0
#define REQUEST_PPM 1
#define REQUEST_3MF 2

// Explore different sortings
#define REQUEST_SINGLE_SORT      0
#define REQUEST_MULTIPLE_SORTS   1

namespace Plater
{
    class Request
    {
        public:
            Request();
            virtual ~Request();

            // Set the plate dimension
            void setPlateSize(float w, float h);

            // Read plater.conf
            void readFromStdin();
            void readFromFile(std::string filename);
            void readPartsFromString(std::string parts);
            void process();
            // Solve only (populate `solution`), without writing output files.
            void solve();
            // Grow the plate size from `idealMm` toward the configured plate
            // size (the physical maximum), in `stepMm` increments, looking for
            // the smallest plate that fits the parts in `targetPlates` plates.
            // If nothing fits within `targetPlates`, the plate count is grown
            // and the search restarts. Writes the chosen solution's files.
            void processFit(double idealMm, double stepMm, int targetPlates);

            // Plate mode (rectangular or circular)
            int plateMode;
            // Plates dimensions
            double plateWidth, plateHeight;
            double plateDiameter;
            // Number of random iterations
            int randomIterations;
            // Output mode (STL or PPM)
            int mode;
            // Sort mode (simple or multiple)
            int sortMode;
            // Precision 
            float precision;
            // Brute-force deltas
            float delta, deltaR;
            // Parts spacing
            float spacing;
            // Output file pattern
            std::string pattern;

            void writeFiles(Solution *solution);
            void writeSTL(Plate *plate, const char *filename);
            void write3MF(Solution *solution, const char *filename);
            void writePpm(Plate *plate, const char *filename);
            void writePlatesInfo(Solution *solution);
        
            std::map<std::string, int> quantities;
            std::map<std::string, Part*> parts;
    
            // Request error
            bool hasError;
            std::string error;

            // Cancel the running request
            bool cancel;
            // Number of plates
            int plates;
            std::vector<std::string> generatedFiles;

            // Stats & solution
            int placersCount, placerCurrent;
            Solution *solution;

            // Enable the plates.csv file output
            bool platesInfo;

            // Number of threads
            unsigned int nbThreads;

            // Use the skyline (bottom-left drop) placement heuristic instead of
            // the full brute-force position grid (rectangular plates only).
            bool skyline;

            // Also try max-contact scored skyline placements (denser packing).
            // Only used together with skyline.
            bool contact;

            // Hole-aware pruned brute force: same placements as the default
            // brute force (fills holes/cavities) but with score-based pruning.
            bool prunedBrute;

            // Post-pass: try to empty the sparsest plate by re-placing its
            // parts into the gaps of the others; kept only if plates drop.
            bool consolidate;

            // Try to reduce the plate count of `solution` in place. Safe: only
            // applied when it strictly reduces the number of plates.
            void consolidateSolution();

        protected:
            void addPart(std::string filename, int quantity, std::string orientation);
            std::vector<std::string> getChunks(string line);
            void readParts();
            std::string readLine();
            std::istream *stream;
            // The raw parts specification, kept so the parts can be reloaded
            // at different plate sizes during processFit().
            std::string partsText;
    };
}

#endif
