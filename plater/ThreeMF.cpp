#include <cmath>
#include <cstdio>
#include <sstream>
#include <vector>
#include "ThreeMF.h"
#include "Zip.h"
#include "Plate.h"
#include "PlacedPart.h"
#include "stl/Model.h"

namespace Plater
{
    // OrcaSlicer / BambuStudio lay plates out on a grid whose stride is
    // (1 + 1/5) times the plate size (see PartPlateList::plate_stride_x and
    // LOGICAL_PART_PLATE_GAP in OrcaSlicer's PartPlate sources).
    static const double LOGICAL_PART_PLATE_GAP = 1.0 / 5.0;

    // Number of plate columns before wrapping to a new row, matching
    // OrcaSlicer's compute_colum_count().
    static int computeColumnCount(int count)
    {
        double value = sqrt((double)count);
        double rounded = round(value);
        return (value > rounded) ? (int)rounded + 1 : (int)rounded;
    }

    static std::string xmlEscape(const std::string &s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '&':  out += "&amp;";  break;
                case '<':  out += "&lt;";   break;
                case '>':  out += "&gt;";   break;
                case '"':  out += "&quot;"; break;
                case '\'': out += "&apos;"; break;
                default:   out += c;        break;
            }
        }
        return out;
    }

    // Format a coordinate (internal units) into millimeters.
    static std::string mm(double v)
    {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%.6f", v / 1000.0);
        return std::string(buffer);
    }

    // Append a <mesh> element for a baked model as a triangle soup (three
    // vertices per triangle). Slicers de-duplicate vertices on import.
    static void appendMesh(std::ostream &os, const Model &model)
    {
        os << "<mesh><vertices>";
        for (const auto &volume : model.volumes) {
            for (const auto &face : volume.faces) {
                for (int i = 0; i < 3; i++) {
                    os << "<vertex x=\"" << mm(face.v[i].x)
                       << "\" y=\"" << mm(face.v[i].y)
                       << "\" z=\"" << mm(face.v[i].z) << "\"/>";
                }
            }
        }
        os << "</vertices><triangles>";
        int idx = 0;
        for (const auto &volume : model.volumes) {
            for (size_t i = 0; i < volume.faces.size(); i++) {
                os << "<triangle v1=\"" << idx
                   << "\" v2=\"" << (idx + 1)
                   << "\" v3=\"" << (idx + 2) << "\"/>";
                idx += 3;
            }
        }
        os << "</triangles></mesh>";
    }

    bool saveSolutionTo3MF(const std::string &filename, Solution *solution,
            double plateWidth, double plateDepth)
    {
        // Collect the non-empty plates; OrcaSlicer numbers plates contiguously.
        std::vector<Plate*> plates;
        for (int i = 0; i < solution->countPlates(); i++) {
            Plate *plate = solution->getPlate(i);
            if (!plate->parts.empty()) {
                plates.push_back(plate);
            }
        }
        if (plates.empty()) {
            return false;
        }

        int cols = computeColumnCount((int)plates.size());
        double strideX = plateWidth * (1.0 + LOGICAL_PART_PLATE_GAP);
        double strideY = plateDepth * (1.0 + LOGICAL_PART_PLATE_GAP);

        std::ostringstream model;     // 3D/3dmodel.model
        std::ostringstream settings;  // Metadata/model_settings.config

        model << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        model << "<model unit=\"millimeter\" xml:lang=\"en-US\" "
                 "xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\" "
                 "xmlns:BambuStudio=\"http://schemas.bambulab.com/package/2021\">\n";
        // OrcaSlicer only restores multi-plate layouts when the generating
        // application identifies as OrcaSlicer/BambuStudio: it checks that this
        // string starts with "OrcaSlicer-" and Semver-parses the remainder.
        // Otherwise the plate definitions in model_settings.config are ignored
        // and everything lands on one plate. Keep the value a clean version.
        model << "<metadata name=\"Application\">OrcaSlicer-2.3.0</metadata>\n";
        model << "<metadata name=\"BambuStudio:3mfVersion\">1</metadata>\n";
        model << "<metadata name=\"Designer\">Plater</metadata>\n";
        model << "<resources>\n";

        settings << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<config>\n";

        int objectId = 1;
        int partId = 1;
        std::vector<std::vector<int>> plateObjectIds(plates.size());

        for (size_t p = 0; p < plates.size(); p++) {
            Plate *plate = plates[p];
            int row = (int)p / cols;
            int col = (int)p % cols;
            // Plate origin on OrcaSlicer's grid (internal units).
            double originX = col * strideX * 1000.0;
            double originY = -row * strideY * 1000.0;

            // Bake each part's positioned model (plate-local coordinates) so we
            // can measure the combined bounding box and center it on the plate.
            std::vector<Model> models;
            models.reserve(plate->parts.size());
            for (auto placed : plate->parts) {
                models.push_back(placed->createModel());
            }

            double minX = 0, minY = 0, maxX = 0, maxY = 0;
            bool first = true;
            for (auto &pm : models) {
                Point3 lo = pm.min();
                Point3 hi = pm.max();
                if (first) {
                    minX = lo.x; minY = lo.y; maxX = hi.x; maxY = hi.y;
                    first = false;
                } else {
                    if (lo.x < minX) minX = lo.x;
                    if (lo.y < minY) minY = lo.y;
                    if (hi.x > maxX) maxX = hi.x;
                    if (hi.y > maxY) maxY = hi.y;
                }
            }

            // Plater packs parts into the (0,0) corner; shift so the combined
            // bounding box is centered on the plate, then add the grid origin.
            double shiftX = originX + plateWidth * 1000.0 / 2.0 - (minX + maxX) / 2.0;
            double shiftY = originY + plateDepth * 1000.0 / 2.0 - (minY + maxY) / 2.0;

            for (size_t k = 0; k < models.size(); k++) {
                Model m = models[k].translate(shiftX, shiftY, 0);
                int id = objectId++;
                std::string name = xmlEscape(plate->parts[k]->getName());

                model << "<object id=\"" << id << "\" type=\"model\" name=\""
                      << name << "\">";
                appendMesh(model, m);
                model << "</object>\n";

                settings << "  <object id=\"" << id << "\">\n";
                settings << "    <metadata key=\"name\" value=\"" << name << "\"/>\n";
                settings << "    <metadata key=\"extruder\" value=\"1\"/>\n";
                settings << "    <part id=\"" << partId++ << "\" subtype=\"normal_part\">\n";
                settings << "      <metadata key=\"name\" value=\"" << name << "\"/>\n";
                settings << "      <metadata key=\"matrix\" value=\"1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1\"/>\n";
                settings << "      <mesh_stat edges_fixed=\"0\" degenerate_facets=\"0\" "
                            "facets_removed=\"0\" facets_reversed=\"0\" backwards_edges=\"0\"/>\n";
                settings << "    </part>\n";
                settings << "  </object>\n";

                plateObjectIds[p].push_back(id);
            }
        }

        // The geometry is baked in global coordinates, so every build item
        // uses an identity transform.
        model << "</resources>\n<build>\n";
        for (const auto &ids : plateObjectIds) {
            for (int id : ids) {
                model << "<item objectid=\"" << id
                      << "\" transform=\"1 0 0 0 1 0 0 0 1 0 0 0\" printable=\"1\"/>\n";
            }
        }
        model << "</build>\n</model>\n";

        // Plate definitions (object -> plate membership and names).
        for (size_t p = 0; p < plates.size(); p++) {
            settings << "  <plate>\n";
            settings << "    <metadata key=\"plater_id\" value=\"" << (p + 1) << "\"/>\n";
            settings << "    <metadata key=\"plater_name\" value=\"" << (p + 1)
                     << " of " << plates.size() << "\"/>\n";
            settings << "    <metadata key=\"locked\" value=\"false\"/>\n";
            for (int id : plateObjectIds[p]) {
                settings << "    <model_instance>\n";
                settings << "      <metadata key=\"object_id\" value=\"" << id << "\"/>\n";
                settings << "      <metadata key=\"instance_id\" value=\"0\"/>\n";
                settings << "    </model_instance>\n";
            }
            settings << "  </plate>\n";
        }
        settings << "</config>\n";

        const std::string contentTypes =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
            "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
            "<Default Extension=\"model\" ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/>"
            "</Types>";

        const std::string rels =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "<Relationship Target=\"/3D/3dmodel.model\" Id=\"rel0\" "
            "Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\"/>"
            "</Relationships>";

        // A project_settings.config is required for OrcaSlicer to treat the
        // file as a project and restore the plates: the loader only rebuilds
        // plates when its loaded config is non-empty (otherwise it falls back
        // to importing geometry onto a single plate). We declare the bed
        // (printable_area) to match the plate grid so the plates line up.
        char dimW[32], dimH[32];
        snprintf(dimW, sizeof(dimW), "%g", plateWidth);
        snprintf(dimH, sizeof(dimH), "%g", plateDepth);
        std::ostringstream project;
        project << "{\n";
        project << "  \"filament_colour\": [\"#5B9BD5\"],\n";
        project << "  \"printable_area\": ["
                << "\"0x0\", "
                << "\"" << dimW << "x0\", "
                << "\"" << dimW << "x" << dimH << "\", "
                << "\"0x" << dimH << "\"],\n";
        project << "  \"printable_height\": \"250\",\n";
        project << "  \"version\": \"2.3.0\"\n";
        project << "}\n";

        Zip zip(filename);
        if (!zip.ok()) {
            return false;
        }
        zip.add("[Content_Types].xml", contentTypes);
        zip.add("_rels/.rels", rels);
        zip.add("3D/3dmodel.model", model.str());
        zip.add("Metadata/model_settings.config", settings.str());
        zip.add("Metadata/project_settings.config", project.str());
        return zip.close();
    }
}
